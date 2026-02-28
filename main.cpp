#include <cassert>
#include <iostream>
#include <vector>
#include <format>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <SDL3/SDL.h>

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define MAX_ENTITIES 1024
#define MAX_EVENTS 1024

enum class ExecutionMode {
    GAME = 0,
    REPLAY = 1,
};

constexpr auto EXECUTION_MODE = ExecutionMode::REPLAY;
// constexpr auto mode = ExecutionMode::GAME;
constexpr float REPLAY_SPEED_MODIFIER = 4.0f;

constexpr int TARGET_FPS = 60;
constexpr double TARGET_FRAME_TIME_SECONDS = 1.0 / TARGET_FPS;
constexpr Uint64 TARGET_FRAME_TIME_NS = 16667;

enum class EntityKind {
    RECTANGLE = 0,
    CIRCLE = 1,
    // ... I guess more?
};

struct RectangleData {
    SDL_FRect rect;
};

struct CircleData {
    // nothing really, who likes circles even
};

struct Entity {
    EntityKind kind;
    bool enabled;

    union {
        RectangleData rectangle;
        CircleData circle;
    } data;
};

struct GameState {
    Entity entities[MAX_ENTITIES];
};

enum class EventKind {
    ADD_RECTANGLE = 0,
    END = 1,
};

struct AddRectangleData {
    SDL_FRect rect;
};

struct Event {
    EventKind kind;
    Uint64 timestamp;

    union {
        AddRectangleData rectangle;
    } data;
};

struct Context {
    double game_logic_fixed_dt;
    double accumulator;
    int next_free_event_index;
    int next_to_process_event_index;
    Uint64 frame_timestamp;

    GameState game_state;
    Event events[MAX_EVENTS];
};

int first_free_entity(const GameState *game_state) {
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        if (!game_state->entities[i].enabled) {
            return i;
        }
    }
    // todo worry about me later
    return -1;
}


bool to_text(const Event *events, std::string &text) {
    text = "";
    for (int i = 0; i < MAX_EVENTS; ++i) {
        const auto &[kind, timestamp, data] = events[i];
        // todo worry about me later perf
        if (timestamp != 0) {
            if (kind == EventKind::ADD_RECTANGLE) {
                text += std::format("{},{},{:.6f},{:.6f},{:.6f},{:.6f}\n",
                                    timestamp,
                                    static_cast<int>(kind),
                                    data.rectangle.rect.x,
                                    data.rectangle.rect.y,
                                    data.rectangle.rect.w,
                                    data.rectangle.rect.h
                );
            } else if (kind == EventKind::END) {
                text += std::format("{},{}\n",
                                    timestamp,
                                    static_cast<int>(kind)
                );
            }
        }
    }
    return true;
}

bool from_text(const std::string &text, Event *events) {
    std::fill_n(events, MAX_EVENTS, Event{});
    std::istringstream iss(text);
    std::string line;
    int index = 0;

    while (std::getline(iss, line) && index < MAX_EVENTS) {
        if (line.empty()) continue;

        std::istringstream line_ss(line);
        uint64_t timestamp = 0;
        int kind = 0;
        float x = 0, y = 0, w = 0, h = 0;
        char c1, c2, c3, c4, c5;

        line_ss >> timestamp >> c1 >> kind;

        if (EXECUTION_MODE == ExecutionMode::REPLAY) {
            timestamp /= REPLAY_SPEED_MODIFIER;
        }

        auto typedKind = static_cast<EventKind>(kind);
        if (typedKind == EventKind::ADD_RECTANGLE) {
            line_ss >> c2 >>
                    x >> c3 >>
                    y >> c4 >>
                    w >> c5 >>
                    h;

            auto &ev = events[index++];
            ev.timestamp = timestamp;
            ev.kind = typedKind;
            ev.data.rectangle.rect.x = x;
            ev.data.rectangle.rect.y = y;
            ev.data.rectangle.rect.w = w;
            ev.data.rectangle.rect.h = h;
        } else if (typedKind == EventKind::END) {
            auto &ev = events[index++];
            ev.timestamp = timestamp;
            ev.kind = typedKind;
        }
    }

    return true;
}

bool save_events_to_file(const Event *events) {
    namespace fs = std::filesystem;
    const auto datetime = std::format("{:%Y-%m-%d_%H_%M_%OS}", std::chrono::system_clock::now());
    const fs::path output_dir = fs::current_path().parent_path().parent_path() / "resources";
    const fs::path file_path = output_dir / ("events_" + datetime + ".txt");
    std::cout << "Saving to file " << file_path.string() << std::endl;
    std::error_code ec;
    create_directories(output_dir, ec);
    assert(!ec);
    std::ofstream file(file_path, std::ios::out | std::ios::trunc);
    std::string file_content;
    bool result = to_text(events, file_content);
    assert(result);
    file << file_content;
    file.close();
    return true;
}

void handle_input(Context *context, bool &running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                context->events[context->next_free_event_index++] = {
                    .kind = EventKind::END,
                    .timestamp = context->frame_timestamp,
                    .data = {}
                };
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            context->events[context->next_free_event_index++] = {
                .kind = EventKind::ADD_RECTANGLE,
                .timestamp = context->frame_timestamp,
                .data = {
                    .rectangle = SDL_FRect{
                        event.button.x,
                        event.button.y,
                        30,
                        30,
                    }
                }
            };
        }
    }
}

void process_event(const Event *event, Context *context, bool &running) {
    if (event->kind == EventKind::ADD_RECTANGLE) {
        const int index = first_free_entity(&context->game_state);
        context->game_state.entities[index].enabled = true;
        context->game_state.entities[index].kind = EntityKind::RECTANGLE;
        context->game_state.entities[index].data.rectangle = RectangleData{
            .rect = event->data.rectangle.rect
        };
    } else if (event->kind == EventKind::END) {
        if (EXECUTION_MODE == ExecutionMode::GAME) {
            save_events_to_file(context->events);
        }
        running = false;
    } else {
        assert(false);
    }
}

void handle_render(Context *context, SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
    for (auto &entity: context->game_state.entities) {
        const auto [kind, enabled, data] = entity;
        if (enabled) {
            if (kind == EntityKind::RECTANGLE) {
                SDL_RenderFillRect(renderer, &data.rectangle.rect);
            } else if (kind == EntityKind::CIRCLE) {
                // fook'em circles, no one likes them anyway
            }
        }
    }

    SDL_RenderPresent(renderer);
}

bool read_latest_events_file(std::string &output) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::current_path().parent_path().parent_path() / "resources";

    if (!exists(dir) || !is_directory(dir)) {
        return false;
    }

    fs::file_time_type latest_time;
    fs::path latest_path;
    bool found_any = false;

    for (const auto &entry: fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto t = entry.last_write_time();
        if (!found_any || t > latest_time) {
            latest_time = t;
            latest_path = entry.path();
            found_any = true;
        }
    }

    if (!found_any) {
        return false;
    }

    std::ifstream file(latest_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    output.assign((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());

    return true;
}


void simulate_input_moving_event_pointers(Context *context) {
    while (context->events[context->next_free_event_index].timestamp < context->frame_timestamp) {
        ++context->next_free_event_index;
    }
}

int main(int argc, char *argv[]) {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_SetAppMetadata("Deterministic Editor for Rectangles", "1.0", "wookash.rectangles");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }


    if (!SDL_CreateWindowAndRenderer("examples/renderer/rectangles",
                                     WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderLogicalPresentation(renderer,
                                     WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    bool running = true;

    const auto context = new Context{};

    context->game_logic_fixed_dt = 1.0 / 60.0;

    if (EXECUTION_MODE == ExecutionMode::REPLAY) {
        // we're gonna reuse context events[] with fake events - nasty
        std::string content;
        read_latest_events_file(content);
        from_text(content, context->events);

        context->game_logic_fixed_dt = 1.0 / (60.0 * REPLAY_SPEED_MODIFIER);
    }

    const Uint64 zero = SDL_GetPerformanceCounter();
    Uint64 last_now = zero;

    while (running) {
        const Uint64 now = SDL_GetPerformanceCounter();
        context->frame_timestamp = now - zero;
        const double frame_time_s = static_cast<double>(now - last_now) / SDL_GetPerformanceFrequency();
        std::cout << "frame time s: " << frame_time_s << std::endl;
        last_now = now;
        context->accumulator += frame_time_s;

        if (EXECUTION_MODE == ExecutionMode::GAME) {
            handle_input(context, running);
        } else if (EXECUTION_MODE == ExecutionMode::REPLAY) {
            simulate_input_moving_event_pointers(context);
        } else {
            assert(false);
        }

        // update - churn through events
        while (context->accumulator >= context->game_logic_fixed_dt) {
            while (context->next_free_event_index > context->next_to_process_event_index) {
                process_event(&context->events[context->next_to_process_event_index], context, running);
                ++context->next_to_process_event_index;
            }
            context->accumulator -= context->game_logic_fixed_dt;
        }

        handle_render(context, renderer);

        SDL_Delay(100);
    }

    delete context;

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
