#include "ggml/ggml.h"

#include "utils.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "gptj.h"

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <signal.h>
#include <unistd.h>
#elif defined (_WIN32)
#include <signal.h>
#include <Windows.h>
#endif

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"

static bool is_interacting = false;

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__)) || defined (_WIN32)
void sigint_handler(int signo) {
    printf(ANSI_COLOR_RESET);
    if (signo == SIGINT) {
        if (!is_interacting) {
            is_interacting=true;
        } else {
            _exit(130);
        }
    }
}
#endif

const char * print_system_info(void) {
    static std::string s;

    s  = "";
    s += "AVX = "       + std::to_string(ggml_cpu_has_avx())       + " | ";
    s += "AVX2 = "      + std::to_string(ggml_cpu_has_avx2())      + " | ";
    s += "AVX512 = "    + std::to_string(ggml_cpu_has_avx512())    + " | ";
    s += "FMA = "       + std::to_string(ggml_cpu_has_fma())       + " | ";
    s += "NEON = "      + std::to_string(ggml_cpu_has_neon())      + " | ";
    s += "ARM_FMA = "   + std::to_string(ggml_cpu_has_arm_fma())   + " | ";
    s += "F16C = "      + std::to_string(ggml_cpu_has_f16c())      + " | ";
    s += "FP16_VA = "   + std::to_string(ggml_cpu_has_fp16_va())   + " | ";
    s += "WASM_SIMD = " + std::to_string(ggml_cpu_has_wasm_simd()) + " | ";
    s += "BLAS = "      + std::to_string(ggml_cpu_has_blas())      + " | ";
    s += "SSE3 = "      + std::to_string(ggml_cpu_has_sse3())      + " | ";
    s += "VSX = "       + std::to_string(ggml_cpu_has_vsx())       + " | ";

    return s.c_str();
}


extern "C" int cpp_main(int argc, char ** argv) {
    ggml_time_init();
    const int64_t t_main_start_us = ggml_time_us();

    gpt_params params;

    if (gpt_params_parse(argc, argv, params) == false) {
        return 1;
    }

    if (params.seed < 0) {
        params.seed = time(NULL);
    }

    fprintf(stderr, "%s: seed = %d\n", __func__, params.seed);

    std::mt19937 rng(params.seed);
    // if (params.prompt.empty()) {
    //     params.prompt = gpt_random_prompt(rng);
    // }

//    params.prompt = R"(// this function checks if the number n is prime
//bool is_prime(int n) {)";

    int64_t t_load_us = 0;

    gpt_vocab vocab;
    gptj_model model;

    // load the model
    {
        const int64_t t_start_us = ggml_time_us();
        if (!gptj_model_load(params.model, model, vocab)) {  
            fprintf(stderr, "%s: failed to load model from '%s'\n", __func__, params.model.c_str());
            return 1;
        }

        t_load_us = ggml_time_us() - t_start_us;
    }

    // print system information
    {
        fprintf(stderr, "\n");
        fprintf(stderr, "system_info: n_threads = %d / %d | %s\n",
                params.n_threads, std::thread::hardware_concurrency(), print_system_info());
    }

    int n_past = 0;

    int64_t t_sample_us  = 0;
    int64_t t_predict_us = 0;

    std::vector<float> logits;

    std::vector<gpt_vocab::id> embd_inp;

    // params.n_predict = std::min(params.n_predict, model.hparams.n_ctx - (int) embd_inp.size());


    std::vector<gpt_vocab::id> instruct_inp = ::gpt_tokenize(vocab, " Below is an instruction that describes a task. Write a response that appropriately completes the request.\n\n");
    std::vector<gpt_vocab::id> prompt_inp = ::gpt_tokenize(vocab, "### Instruction:\n\n");
    std::vector<gpt_vocab::id> response_inp = ::gpt_tokenize(vocab, "### Response:\n\n");
    embd_inp.insert(embd_inp.end(), instruct_inp.begin(), instruct_inp.end());

    if(!params.prompt.empty()) {
        std::vector<gpt_vocab::id> param_inp = ::gpt_tokenize(vocab, params.prompt);
        embd_inp.insert(embd_inp.end(), prompt_inp.begin(), prompt_inp.end());
        embd_inp.insert(embd_inp.end(), param_inp.begin(), param_inp.end());
        embd_inp.insert(embd_inp.end(), response_inp.begin(), response_inp.end());
    }

    // fprintf(stderr, "\n");
    // fprintf(stderr, "%s: prompt: '%s'\n", __func__, params.prompt.c_str());
    // fprintf(stderr, "%s: number of tokens in prompt = %zu\n", __func__, embd_inp.size());
    // for (int i = 0; i < (int) embd_inp.size(); i++) {
    //     fprintf(stderr, "%6d -> '%s'\n", embd_inp[i], vocab.id_to_token.at(embd_inp[i]).c_str());
    // }
    // fprintf(stderr, "\n");

    if (params.interactive) {
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
        struct sigaction sigint_action;
        sigint_action.sa_handler = sigint_handler;
        sigemptyset (&sigint_action.sa_mask);
        sigint_action.sa_flags = 0;
        sigaction(SIGINT, &sigint_action, NULL);
#elif defined (_WIN32)
        signal(SIGINT, sigint_handler);

        // Windows console ANSI color fix
        DWORD mode = 0;
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole && hConsole != INVALID_HANDLE_VALUE && GetConsoleMode(hConsole, &mode)){
            SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            SetConsoleOutputCP(CP_UTF8);
        }
#endif

        fprintf(stderr, "%s: interactive mode on.\n", __func__);

        // if(antiprompt_inp.size()) {
        //     fprintf(stderr, "%s: reverse prompt: '%s'\n", __func__, params.antiprompt.c_str());
        //     fprintf(stderr, "%s: number of tokens in reverse prompt = %zu\n", __func__, antiprompt_inp.size());
        //     for (int i = 0; i < (int) antiprompt_inp.size(); i++) {
        //         fprintf(stderr, "%6d -> '%s'\n", antiprompt_inp[i], vocab.id_to_token.at(antiprompt_inp[i]).c_str());
        //     }
        //     fprintf(stderr, "\n");
        // }
    }
    fprintf(stderr, "sampling parameters: temp = %f, top_k = %d, top_p = %f\n", params.temp, params.top_k, params.top_p);
    fprintf(stderr, "\n\n");

    std::vector<gpt_vocab::id> embd;

    // determine the required inference memory per token:
    size_t mem_per_token = 0;
    gptj_eval(model, params.n_threads, 0, { 0, 1, 2, 3 }, logits, mem_per_token);


//
    int last_n_size = 64;
    std::vector<gpt_vocab::id> last_n_tokens(last_n_size);
    std::fill(last_n_tokens.begin(), last_n_tokens.end(), 0);


    if (params.interactive) {
        fprintf(stderr, "== Running in chat mode. ==\n"
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__)) || defined (_WIN32)
               " - Press Ctrl+C to interject at any time.\n"
#endif
               " - Press Return to return control to LLaMA.\n"
               " - If you want to submit another line, end your input in '\\'.\n");
    }

    // we may want to slide the input window along with the context, but for now we restrict to the context length
    int remaining_tokens = model.hparams.n_ctx - embd_inp.size();
    int input_consumed = 0;
    bool input_noecho = true;

    // prompt user immediately after the starting prompt has been loaded
    if (params.interactive_start) {
        is_interacting = true;
    }

    // set the color for the prompt which will be output initially
    if (params.use_color) {
        printf(ANSI_COLOR_YELLOW);
    }

    while (remaining_tokens > 0) {
        // predict
        if (embd.size() > 0) {
            const int64_t t_start_us = ggml_time_us();

            if (!gptj_eval(model, params.n_threads, n_past, embd, logits, mem_per_token)) {
                fprintf(stderr, "Failed to predict\n");
                return 1;
            }

            t_predict_us += ggml_time_us() - t_start_us;
        }

        n_past += embd.size();
        embd.clear();

        if (embd_inp.size() <= input_consumed && !is_interacting) {
            // out of user input, sample next token
            const float top_k = params.top_k;
            const float top_p = params.top_p;
            const float temp  = params.temp;
            const float repeat_penalty = 1.30f; // model.hparams.repeat_penalty;

            const int n_vocab = model.hparams.n_vocab;

            gpt_vocab::id id = 0;

            {
                const int64_t t_start_sample_us = ggml_time_us();

                // id = gpt_sample_top_k_top_p(vocab, logits.data() + (logits.size() - n_vocab), last_n_tokens, repeat_penalty, top_k, top_p, temp, rng);
                id = gpt_sample_top_k_top_p(vocab, logits.data() + (logits.size() - n_vocab), top_k, top_p, temp, rng);

                last_n_tokens.erase(last_n_tokens.begin());
                last_n_tokens.push_back(id);

                t_sample_us += ggml_time_us() - t_start_sample_us;
            }

            // add it to the context
            embd.push_back(id);

            // echo this to console
            input_noecho = false;

            // decrement remaining sampling budget
            --remaining_tokens;
        } else {
            // some user input remains from prompt or interaction, forward it to processing
            while (embd_inp.size() > input_consumed) {
                // fprintf(stderr, "%6d -> '%s'\n", embd_inp[input_consumed], vocab.id_to_token.at(embd_inp[input_consumed]).c_str());

                embd.push_back(embd_inp[input_consumed]);
                last_n_tokens.erase(last_n_tokens.begin());
                last_n_tokens.push_back(embd_inp[input_consumed]);
                ++input_consumed;
                if (embd.size() > params.n_batch) {
                    break;
                }
            }

            // reset color to default if we there is no pending user input
            if (!input_noecho && params.use_color && embd_inp.size() == input_consumed) {
                printf(ANSI_COLOR_RESET);
            }
        }

        // display text
        if (!input_noecho) {
            for (auto id : embd) {
                if (id != 50256) 
                    printf("%s", vocab.id_to_token[id].c_str());
            }
            fflush(stdout);
        }

        // in interactive mode, and not currently processing queued inputs;
        // check if we should prompt the user for more
        if (params.interactive && embd_inp.size() <= input_consumed) {
            // check for reverse prompt
            // if (antiprompt_inp.size() && std::equal(antiprompt_inp.rbegin(), antiprompt_inp.rend(), last_n_tokens.rbegin())) {
            //     // reverse prompt found
            //     is_interacting = true;
            // }
            if (is_interacting) {
                // input_consumed =  0;
                // embd_inp.erase(embd_inp.begin());
                input_consumed = embd_inp.size();
                embd_inp.insert(embd_inp.end(), prompt_inp.begin(), prompt_inp.end());
                

                // set the color for the prompt which will be output initially
                if (params.use_color) {
                    printf(ANSI_COLOR_YELLOW);
                }
                printf("\n> ");

                // currently being interactive
                bool another_line=true;
                while (another_line) {
                    fflush(stdout);
                    char buf[256] = {0};
                    int n_read;
                    if(params.use_color) printf(ANSI_BOLD ANSI_COLOR_GREEN);
                    if (scanf("%255[^\n]%n%*c", buf, &n_read) <= 0) {
                        // presumable empty line, consume the newline
                        if (scanf("%*c") <= 0) { /*ignore*/ }
                        n_read=0;
                    }
                    if(params.use_color) printf(ANSI_COLOR_RESET);

                    if (n_read > 0 && buf[n_read-1]=='\\') {
                        another_line = true;
                        buf[n_read-1] = '\n';
                        buf[n_read] = 0;
                    } else {
                        another_line = false;
                        buf[n_read] = '\n';
                        buf[n_read+1] = 0;
                    }

                    std::vector<gpt_vocab::id> line_inp = ::gpt_tokenize(vocab, buf);
                    embd_inp.insert(embd_inp.end(), line_inp.begin(), line_inp.end());
                    embd_inp.insert(embd_inp.end(), response_inp.begin(), response_inp.end());

                    remaining_tokens -= prompt_inp.size() + line_inp.size() + response_inp.size();

                    input_noecho = true; // do not echo this again
                }

                is_interacting = false;
            }
        }

        // end of text token
        if (embd.back() == 50256) {
            if (params.interactive) {
                is_interacting = true;
                continue;
            } else {
                printf("\n");
                fprintf(stderr, " [end of text]\n");
                break;
            }
        }
    }

#if defined (_WIN32)
    signal(SIGINT, SIG_DFL);
#endif

    // report timing
    {
        const int64_t t_main_end_us = ggml_time_us();

        fprintf(stderr, "\n\n");
        fprintf(stderr, "%s: mem per token = %8zu bytes\n", __func__, mem_per_token);
        fprintf(stderr, "%s:     load time = %8.2f ms\n", __func__, t_load_us/1000.0f);
        fprintf(stderr, "%s:   sample time = %8.2f ms\n", __func__, t_sample_us/1000.0f);
        fprintf(stderr, "%s:  predict time = %8.2f ms / %.2f ms per token\n", __func__, t_predict_us/1000.0f, t_predict_us/1000.0f/n_past);
        fprintf(stderr, "%s:    total time = %8.2f ms\n", __func__, (t_main_end_us - t_main_start_us)/1000.0f);
    }

    ggml_free(model.ctx);

    if (params.use_color) {
        printf(ANSI_COLOR_RESET);
    }

    return 0;
}
