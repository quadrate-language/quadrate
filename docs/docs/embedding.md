# Embedding Quadrate

This guide shows how to embed Quadrate as a scripting language in your C/C++ applications. Quadrate is well-suited for embedding because it:

- Compiles to native code (fast execution)
- Has a simple stack-based model (easy to interface with)
- Provides a clean C API
- Supports registering native functions

## Quick Start

Here's the minimal code to embed Quadrate:

```c
#include <quadrate/qd/qd.h>

int main(void) {
    // Create execution context with 1024-element stack
    qd_context* ctx = qd_create_context(1024);

    // Create a module and add code
    qd_module* mod = qd_get_module(ctx, "game");
    qd_add_script(mod, "fn hello() { \"Hello from Quadrate!\" print nl }");
    qd_build(mod);

    // Execute
    qd_execute(ctx, "game::hello");

    // Cleanup
    qd_free_context(ctx);
    return 0;
}
```

Compile with:
```bash
DIST=/path/to/quadrate/dist
cc -o myapp myapp.c -I$DIST/include -L$DIST/lib -lqd -lqdrt -Wl,-rpath,$DIST/lib
```

`libqd` and `libqdrt` are shared libraries in `$DIST/lib`; the rpath lets the program find them
at run time without `LD_LIBRARY_PATH`.

## Core Concepts

### Contexts

A `qd_context` holds the runtime state: the stack, error state, and loaded modules.

```c
qd_context* ctx = qd_create_context(1024);  // Stack capacity
// ... use context ...
qd_free_context(ctx);
```

### Modules

Modules are containers for functions. Create or retrieve a module with `qd_get_module`:

```c
qd_module* math = qd_get_module(ctx, "math");
qd_module* game = qd_get_module(ctx, "game");
```

### Adding Scripts

Add Quadrate source code to a module:

```c
qd_add_script(mod, "def square(x:i64 -- result:i64) { x x * }");
qd_add_script(mod, "def cube(x:i64 -- result:i64) { x x * x * }");
```

You can call `qd_add_script` multiple times before building.

### Building

Compile all added scripts:

```c
qd_build(mod);

// Check if compilation succeeded
if (!qd_is_compiled(mod)) {
    fprintf(stderr, "Compilation failed!\n");
}
```

### Executing Code

Execute Quadrate expressions:

```c
qd_execute(ctx, "5 math::square print nl");  // Prints: 25
qd_execute(ctx, "3 math::cube print nl");    // Prints: 27
```

## Registering Native Functions

The real power of embedding comes from exposing your application's functions to scripts.

### Basic Native Function

Native functions take a `qd_context*` and a `void* userdata`, and return `int`. When registering, you provide a stack effect signature so the compiler can type-check calls:

```c
#include <quadrate/qd/qd.h>
#include <quadrate/rt/runtime.h>
#include <time.h>

// Native function: pushes current time onto stack ( -- t:i64)
int native_get_time(qd_context* ctx, void* userdata) {
    time_t now = time(NULL);
    return qd_push_i(ctx, (int64_t)now);
}

int main(void) {
    qd_context* ctx = qd_create_context(1024);
    qd_module* utils = qd_get_module(ctx, "utils");

    // Register with stack effect signature
    qd_register_function(utils, "get_time", "( -- t:i64)", native_get_time, NULL);

    qd_build(utils);

    // Call from Quadrate
    qd_execute(ctx, "utils::get_time print nl");

    qd_free_context(ctx);
    return 0;
}
```

The signature string uses Quadrate stack effect syntax: `(inputs -- outputs)`. Parameter names are documentation-only; the types are what matter:

```c
qd_register_function(mod, "add", "(a:i64 b:i64 -- sum:i64)", my_add, NULL);
qd_register_function(mod, "greet", "(name:str -- msg:str)", my_greet, NULL);
qd_register_function(mod, "reset", "( -- )", my_reset, NULL);
```

### Reading Arguments from the Stack

Use the typed pop functions to read arguments. They handle type checking and string cleanup automatically:

```c
// Native function: (a:i64 b:i64 -- sum:i64)
int native_add(qd_context* ctx, void* userdata) {
    int64_t b, a;
    qd_pop_i(ctx, &b);  // Pop b (top)
    qd_pop_i(ctx, &a);  // Pop a (below b)
    return qd_push_i(ctx, a + b);
}
```

### Pop Functions

```c
int64_t i;  qd_pop_i(ctx, &i);              // Pop integer
double f;   qd_pop_f(ctx, &f);              // Pop float
char buf[256]; qd_pop_s(ctx, buf, sizeof(buf)); // Pop string into buffer
void* p;    qd_pop_p(ctx, &p);              // Pop pointer
```

All pop functions return `QD_OK` on success, or an error code on underflow/type mismatch.
`qd_pop_s` copies the string into your buffer and releases the internal reference automatically.

`qd_pop_p` is for a `ptr`: raw memory, which has nothing to release. A struct, an array or a
function value also arrives as a pointer, and popping it hands you the reference the stack
held: release it with `qd_struct_release` (or `qd_array_release` for an array) when you are
done. For those, prefer `qd_args`, below, which checks every argument's type before it pops
anything.

### Popping All Arguments at Once

`qd_args(ctx, op, sig, ...)` pops every input of a native in one call. `sig` has one letter per
input, deepest first, as the signature writes them: `i` i64, `f` f64, `d` d128, `s` str
(a `qd_string_t*`), `p` a pointer (`ptr`, struct, array or function value) and `v` any value.
Each letter takes a pointer to receive the value. Strings and other counted values come with
the stack's reference, which is now yours to release. On a wrong type or too few values it
raises an error naming `op`, and leaves the stack as it was.

```c
// Native: (a:i64 b:i64 -- sum:i64)
int native_add(qd_context* ctx, void* userdata) {
    int64_t a, b;
    qd_args(ctx, "math::add", "ii", &a, &b);
    return qd_push_i(ctx, a + b);
}
```

### Push Functions

```c
qd_push_i(ctx, 42);           // Push integer
qd_push_f(ctx, 3.14159);      // Push float
qd_push_s(ctx, "hello");      // Push string (copied)
qd_push_p(ctx, my_pointer);   // Push a pointer to raw memory (nothing counted)
```

`qd_push_p` is only for raw memory: a buffer, a C handle. To push a struct or an array use
`qd_push_struct` or `qd_push_array`, which hand the stack the reference you hold.

### Userdata

Each native function receives a `void* userdata` pointer that was passed at registration time. This lets callbacks access application state without globals:

```c
typedef struct { int score; const char* name; } Player;

int get_score(qd_context* ctx, void* userdata) {
    Player* p = (Player*)userdata;
    return qd_push_i(ctx, p->score);
}

Player player = { .score = 42, .name = "Alice" };
qd_register_function(mod, "get_score", "( -- score:i64)", get_score, &player);
```

For data that should be accessible from any native function regardless of which module it belongs to, use context-level userdata:

```c
qd_set_userdata(ctx, &game_state);

// Later, in any native function:
int some_func(qd_context* ctx, void* userdata) {
    GameState* game = qd_get_userdata(ctx);
    // ...
}
```

### Native-Only Modules

You can create modules that consist entirely of registered native functions, with no Quadrate source code. When another module uses such a module, the embedding API automatically generates stub functions so compiled code can call your native functions transparently:

```c
// Register a pure-native module
qd_module* sensors = qd_get_module(ctx, "sensors");
qd_register_function(sensors, "temperature", "( -- v:f64)", read_temp, &hw);
qd_register_function(sensors, "pressure", "( -- v:f64)", read_pressure, &hw);
// No qd_add_script needed. qd_build is only needed to call these words directly
// with qd_execute ("sensors::temperature"); a module that uses them works without it.

// Another module can use it normally
qd_module* app = qd_get_module(ctx, "app");
qd_add_script(app, "use sensors\nfn check() { sensors::temperature print nl }");
qd_build(app);
```

Type signatures enable seamless interop with the Quadrate standard library:

<!-- doccheck: skip the host application registers these words; they cannot exist outside it -->
```quadrate
use sensors
use strconv

fn telemetry( -- ) {
    sensors::temperature strconv::format_float print nl  // type-checked!
}
```

## Interpreted Execution

Everything above compiles through LLVM, which is the right trade for code that runs many times.
For code a person is typing — a console, a REPL, a rule someone edits while the application is
running — there is a second execution path that parses and walks the AST directly, with no code
generation and no toolchain at run time:

```c
#include <quadrate/interp/interp.h>
#include <stdio.h>

int main(void) {
    qd_interp* interp = qd_interp_create(1024);

    qd_interp_eval(interp, "2 3 +");

    qd_interp_value value;
    if (qd_interp_peek(interp, 0, &value)) {
        printf("%s\n", value.text);   // 5
    }

    qd_interp_destroy(interp);
    return 0;
}
```

Compile with:
```bash
DIST=/path/to/quadrate/dist
cc -o myapp myapp.c -I$DIST/include -L$DIST/lib/quadrate -linterp -lqc -lu8t -lrt -lstdc++ -lm
```

The interpreter libraries are static and live in `$DIST/lib/quadrate`; the parser is C++, hence
`-lstdc++`.

The stack persists across calls, which is what a prompt needs:

```c
qd_interp_eval(interp, "2");
qd_interp_eval(interp, "3");
qd_interp_eval(interp, "+");      // 5
```

### Sharing a Context

`qd_interp_attach()` builds an interpreter over a context you already have, so typed lines and
compiled modules work against the same stack. The interpreter borrows the context and will not
free it:

```c
qd_context* ctx = qd_create_context(1024);
qd_module* mod = qd_get_module(ctx, "game");
qd_add_script(mod, "def double(x:i64 -- r:i64) { x 2 * }");
qd_build(mod);

qd_interp* interp = qd_interp_attach(ctx);   // borrows ctx
qd_interp_eval(interp, "21");                // interpreted, leaves 21 on the stack
qd_execute(ctx, "game::double");             // compiled, consumes it

qd_interp_destroy(interp);                   // ctx survives
qd_free_context(ctx);
```

Linking this combination needs both paths, with the shared runtime in place of the static `-lrt`:

```bash
cc -o myapp myapp.c -I$DIST/include -L$DIST/lib -L$DIST/lib/quadrate \
    -linterp -lqc -lu8t -lqd -lqdrt -lstdc++ -lm -Wl,-rpath,$DIST/lib
```

### Errors

Interpreted code is typed by people and is wrong all the time, so nothing on this path ends the
process. Arity is checked before an instruction runs, and everything else — type mismatch,
division by zero, an array index past the end — runs inside a recovery point, so the runtime unwinds
back to the interpreter instead of exiting:

```c
if (!qd_interp_eval(interp, "1 0 /")) {
    printf("%s\n", qd_interp_error(interp));   // div: Division by zero
}
```

The context stays usable afterwards. On failure the stack is left as the error found it rather
than rolled back, which is what a user needs in order to see what went wrong.

### Coverage

The interpreter handles built-in instructions, literals, control flow (`if`, `for`, `switch`),
locals (`-> x`) and function values. A line may also be nothing but declarations, such as
`def sq(x:i64 -- r:i64) { x x * }`; the function is then callable from later lines. A line that
mixes declarations with code to run is an error, so define in one call and use in the next.

Module imports (`use strings`) are not interpreted yet, so neither is anything from the
standard library modules; they are reported as an error rather than failing silently. Use
`qd_add_script()` and `qd_build()` for those.

---

---

# Tutorial: Embedding in a Game Engine

This tutorial shows how to use Quadrate as a scripting language for a simple game engine. We'll build a system where:

1. Game entities have scriptable behaviors
2. Scripts can access game state (positions, health, etc.)
3. Scripts can call engine functions (spawn, damage, move)

## Project Structure

```
my_game/
├── src/
│   ├── main.c
│   ├── engine.c
│   ├── engine.h
│   └── scripting.c
├── scripts/
│   ├── player.qd
│   └── enemy.qd
└── Makefile
```

## Step 1: Define Game State

```c
// engine.h
#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_ENTITIES 100

typedef struct {
    int64_t id;
    double x, y;
    int64_t health;
    int64_t max_health;
    bool active;
    const char* script_module;  // Which Quadrate module controls this entity
} Entity;

typedef struct {
    Entity entities[MAX_ENTITIES];
    int entity_count;
    int64_t current_entity;  // Entity being updated (for script access)
    double delta_time;
} GameState;

Entity* engine_spawn(GameState* game, double x, double y, int64_t health, const char* script);
Entity* engine_get_entity(GameState* game, int64_t id);
void engine_damage(GameState* game, int64_t id, int64_t amount);
void engine_move(GameState* game, int64_t id, double dx, double dy);

#endif
```

## Step 2: Implement Engine Functions

```c
// engine.c
#include "engine.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

Entity* engine_spawn(GameState* game, double x, double y, int64_t health, const char* script) {
    if (game->entity_count >= MAX_ENTITIES) return NULL;

    Entity* e = &game->entities[game->entity_count];
    e->id = game->entity_count;
    e->x = x;
    e->y = y;
    e->health = health;
    e->max_health = health;
    e->active = true;
    e->script_module = script;

    game->entity_count++;
    printf("[Engine] Spawned entity %" PRId64 " at (%.1f, %.1f)\n", e->id, x, y);
    return e;
}

Entity* engine_get_entity(GameState* game, int64_t id) {
    if (id < 0 || id >= game->entity_count) return NULL;
    return &game->entities[id];
}

void engine_damage(GameState* game, int64_t id, int64_t amount) {
    Entity* e = engine_get_entity(game, id);
    if (!e || !e->active) return;

    e->health -= amount;
    printf("[Engine] Entity %" PRId64 " took %" PRId64 " damage (health: %" PRId64 ")\n",
           id, amount, e->health);

    if (e->health <= 0) {
        e->active = false;
        printf("[Engine] Entity %" PRId64 " destroyed!\n", id);
    }
}

void engine_move(GameState* game, int64_t id, double dx, double dy) {
    Entity* e = engine_get_entity(game, id);
    if (!e || !e->active) return;

    e->x += dx;
    e->y += dy;
}
```

## Step 3: Create Script Bindings

```c
// scripting.c
#include <quadrate/qd/qd.h>
#include <quadrate/rt/runtime.h>
#include "engine.h"
#include <stdio.h>
#include <math.h>

static qd_context* script_ctx = NULL;

// === Native functions exposed to scripts ===
// Each function receives the GameState* through userdata — no globals needed.

// Get current entity's ID ( -- id)
int script_self(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    return qd_push_i(ctx, game->current_entity);
}

// Get entity position (id -- x y)
int script_get_pos(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    int64_t id;
    qd_pop_i(ctx, &id);

    Entity* e = engine_get_entity(game, id);
    if (!e) {
        qd_push_f(ctx, 0.0);
        qd_push_f(ctx, 0.0);
        return QD_OK;
    }

    qd_push_f(ctx, e->x);
    return qd_push_f(ctx, e->y);
}

// Get entity health (id -- health)
int script_get_health(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    int64_t id;
    qd_pop_i(ctx, &id);

    Entity* e = engine_get_entity(game, id);
    return qd_push_i(ctx, e ? e->health : 0);
}

// Move entity (id dx dy --)
int script_move(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    double dy, dx;
    int64_t id;
    qd_pop_f(ctx, &dy);
    qd_pop_f(ctx, &dx);
    qd_pop_i(ctx, &id);

    engine_move(game, id, dx, dy);
    return QD_OK;
}

// Damage entity (id amount --)
int script_damage(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    int64_t amount, id;
    qd_pop_i(ctx, &amount);
    qd_pop_i(ctx, &id);

    engine_damage(game, id, amount);
    return QD_OK;
}

// Spawn new entity (x y health -- id)
int script_spawn(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    int64_t health;
    double y, x;
    qd_pop_i(ctx, &health);
    qd_pop_f(ctx, &y);
    qd_pop_f(ctx, &x);

    Entity* e = engine_spawn(game, x, y, health, NULL);
    return qd_push_i(ctx, e ? e->id : -1);
}

// Get delta time ( -- dt)
int script_delta_time(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    return qd_push_f(ctx, game->delta_time);
}

// Calculate distance between two points (x1 y1 x2 y2 -- dist)
int script_distance(qd_context* ctx, void* userdata) {
    double y2, x2, y1, x1;
    qd_pop_f(ctx, &y2);
    qd_pop_f(ctx, &x2);
    qd_pop_f(ctx, &y1);
    qd_pop_f(ctx, &x1);

    double dx = x2 - x1;
    double dy = y2 - y1;
    return qd_push_f(ctx, sqrt(dx*dx + dy*dy));
}

// Log a message (msg --)
int script_log(qd_context* ctx, void* userdata) {
    char msg[1024];
    qd_pop_s(ctx, msg, sizeof(msg));
    printf("[Script] %s\n", msg);
    return QD_OK;
}

// === Script system initialization ===

void scripting_init(GameState* game) {
    script_ctx = qd_create_context(4096);

    // Create the 'engine' module — pass game state as userdata to each function
    qd_module* engine = qd_get_module(script_ctx, "engine");

    qd_register_function(engine, "self", "( -- id:i64)", script_self, game);
    qd_register_function(engine, "get_pos", "(id:i64 -- x:f64 y:f64)", script_get_pos, game);
    qd_register_function(engine, "get_health", "(id:i64 -- health:i64)", script_get_health, game);
    qd_register_function(engine, "move", "(id:i64 dx:f64 dy:f64 -- )", script_move, game);
    qd_register_function(engine, "damage", "(id:i64 amount:i64 -- )", script_damage, game);
    qd_register_function(engine, "spawn", "(x:f64 y:f64 health:i64 -- id:i64)", script_spawn, game);
    qd_register_function(engine, "delta_time", "( -- dt:f64)", script_delta_time, game);
    qd_register_function(engine, "distance", "(x1:f64 y1:f64 x2:f64 y2:f64 -- dist:f64)", script_distance, game);
    qd_register_function(engine, "log", "(msg:str -- )", script_log, game);

    // Not needed for the scripts that `use engine`; built so the host can also
    // qd_execute engine words directly
    qd_build(engine);
}

void scripting_load_module(const char* name, const char* source) {
    qd_module* mod = qd_get_module(script_ctx, name);
    qd_add_script(mod, source);
    qd_build(mod);

    if (!qd_is_compiled(mod)) {
        fprintf(stderr, "Failed to compile module: %s\n", name);
    }
}

void scripting_call_update(const char* module_name) {
    char call[256];
    snprintf(call, sizeof(call), "%s::update", module_name);
    qd_execute(script_ctx, call);
}

void scripting_shutdown(void) {
    qd_free_context(script_ctx);
    script_ctx = NULL;
}
```

## Step 4: Write Game Scripts

<!-- doccheck: skip the host application registers these words; they cannot exist outside it -->
```quadrate
// scripts/player.qd
// Player behavior script
use engine

// Called every frame
fn update() {
    engine::self -> id

    // Get current position
    id engine::get_pos -> y -> x

    // Simple movement: move right over time
    engine::delta_time 100.0 * -> speed
    id speed 0.0 engine::move

    // Log position every update
    "Player position: " print x print ", " print y print nl
}

def take_damage(amount:i64 -- ) {
    engine::self -> id
    id amount engine::damage

    id engine::get_health -> hp
    hp 0 > if {
        "Ouch! Health remaining: " print hp print nl
    } else {
        "Player defeated!" engine::log
    }
}
```

<!-- doccheck: skip the host application registers these words; they cannot exist outside it -->
```quadrate
// scripts/enemy.qd
// Enemy AI script
use engine

fn update() {
    engine::self -> id

    // Get own position
    id engine::get_pos -> my_y -> my_x

    // Get player position (assume player is entity 0)
    0 engine::get_pos -> player_y -> player_x

    // Calculate distance to player
    my_x my_y player_x player_y engine::distance -> dist

    // If close enough, attack!
    dist 50.0 < if {
        "Enemy attacks player!" engine::log
        0 10 engine::damage
    } else {
        // Move toward player
        player_x my_x - -> dx
        player_y my_y - -> dy

        // Normalize and scale by speed
        dist 0.001 + -> len  // Avoid divide by zero
        dx len / engine::delta_time * 50.0 * -> move_x
        dy len / engine::delta_time * 50.0 * -> move_y

        id move_x move_y engine::move
    }
}
```

## Step 5: Main Game Loop

```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "engine.h"

// Forward declarations from scripting.c
void scripting_init(GameState* game);
void scripting_load_module(const char* name, const char* source);
void scripting_call_update(const char* module_name);
void scripting_shutdown(void);

// Helper to load script file
char* load_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main(void) {
    printf("=== Game Engine with Quadrate Scripting ===\n\n");

    // Game state lives on the stack — no globals
    GameState game = {0};

    // Initialize scripting system with game state
    scripting_init(&game);

    // Load scripts
    char* player_script = load_file("scripts/player.qd");
    char* enemy_script = load_file("scripts/enemy.qd");

    if (player_script) {
        scripting_load_module("player", player_script);
        free(player_script);
    }
    if (enemy_script) {
        scripting_load_module("enemy", enemy_script);
        free(enemy_script);
    }

    // Spawn entities
    Entity* player = engine_spawn(&game, 0.0, 0.0, 100, "player");
    Entity* enemy = engine_spawn(&game, 100.0, 100.0, 50, "enemy");

    // Game loop (simplified - just 10 frames)
    printf("\n--- Starting game loop ---\n\n");

    for (int frame = 0; frame < 10; frame++) {
        printf("Frame %d:\n", frame);
        game.delta_time = 0.016;  // ~60 FPS

        // Update all entities
        for (int i = 0; i < game.entity_count; i++) {
            Entity* e = &game.entities[i];
            if (!e->active || !e->script_module) continue;

            game.current_entity = e->id;
            scripting_call_update(e->script_module);
        }

        printf("\n");
        usleep(100000);  // 100ms delay for demo
    }

    // Cleanup
    scripting_shutdown();
    printf("=== Game ended ===\n");

    return 0;
}
```

## Step 6: Build and Run

```makefile
# Makefile
QUADRATE_DIR = /path/to/quadrate/dist
CFLAGS = -I$(QUADRATE_DIR)/include -Wall
LDFLAGS = -L$(QUADRATE_DIR)/lib -lqd -lqdrt -lm -Wl,-rpath,$(QUADRATE_DIR)/lib

game: src/main.c src/engine.c src/scripting.c
	gcc $(CFLAGS) -o game $^ $(LDFLAGS)

clean:
	rm -f game
```

## API Reference

### Context Management

| Function | Description |
|----------|-------------|
| `qd_create_context(size)` | Create context with stack capacity |
| `qd_free_context(ctx)` | Free context and all resources |
| `qd_clone_context(ctx)` | Deep copy a context |
| `qd_set_userdata(ctx, ptr)` | Set user-defined data pointer on context |
| `qd_get_userdata(ctx)` | Get user-defined data pointer from context |

### Module Management

| Function | Description |
|----------|-------------|
| `qd_get_module(ctx, name)` | Get or create module |
| `qd_add_script(mod, source)` | Add Quadrate source code |
| `qd_register_function(mod, name, sig, fn, userdata)` | Register native function with type signature |
| `qd_build(mod)` | Compile the module |
| `qd_is_compiled(mod)` | Check if compilation succeeded |

### Execution

| Function | Description |
|----------|-------------|
| `qd_execute(ctx, expr)` | Execute Quadrate expression |

### Interpreted Execution

Declared in `<quadrate/interp/interp.h>`. Link with `-L$DIST/lib/quadrate -linterp -lqc -lu8t -lrt -lstdc++ -lm`.

| Function | Description |
|----------|-------------|
| `qd_interp_create(size)` | Create interpreter with a context of its own |
| `qd_interp_attach(ctx)` | Create interpreter over an existing context (borrowed) |
| `qd_interp_eval(interp, src)` | Parse and execute source against the persistent stack |
| `qd_interp_error(interp)` | Message from the most recent failure |
| `qd_interp_depth(interp)` | Number of values on the stack |
| `qd_interp_peek(interp, i, &v)` | Read a stack value without removing it (0 is the top) |
| `qd_interp_context(interp)` | The context this interpreter executes against |
| `qd_interp_destroy(interp)` | Destroy interpreter; frees the context only if it created it |

### Stack Operations

| Function | Description |
|----------|-------------|
| `qd_push_i(ctx, val)` | Push integer |
| `qd_push_f(ctx, val)` | Push float |
| `qd_push_s(ctx, str)` | Push string (copied) |
| `qd_push_p(ctx, ptr)` | Push a pointer to raw memory (never an array, a struct or a function value), which costs no reference counting |
| `qd_push_struct(ctx, p)`, `qd_push_array(ctx, p)` | Push a struct or an array, handing the stack the reference the caller holds |
| `qd_call_value(ctx, fn)` | Call a function value a native was given, with its inputs on the stack; the caller keeps its reference |
| `qd_closure_new(fn, NULL, 0)` | Make a function value of a plain C function `int fn(qd_context*)`, held with one reference |
| `qd_closure_new(fn, env, n)` | The same with an environment: the function is `int fn(qd_context*, void* env)` |
| `qd_struct_release(p)` | Give up one reference to a struct or function value |
| `qd_args(ctx, op, sig, ...)` | Pop all of a native's inputs, typed by `sig` (`i f d s p v`); counted values come with their reference |
| `qd_fail(ctx, code, fmt, ...)` | Fail a fallible (`!`) native: sets the error and pushes the status; `return` its result |
| `qd_pop_i(ctx, &val)` | Pop integer |
| `qd_pop_f(ctx, &val)` | Pop float |
| `qd_pop_s(ctx, buf, size)` | Pop string into buffer |
| `qd_pop_p(ctx, &val)` | Pop a pointer; a counted value's reference becomes the caller's to release |
| `qd_context_stack_size(ctx)` | Get number of elements on stack |

### Error Inspection

| Function | Description |
|----------|-------------|
| `qd_error_code(ctx)` | Get current error code (0 = no error) |
| `qd_error_message(ctx)` | Get current error message |
| `qd_clear_error(ctx)` | Reset error state |

## Best Practices

1. **Error Handling**: Always check `qd_is_compiled()` after building modules. Check return codes from `qd_pop_*` functions.

2. **Stack Balance**: Native functions must leave the stack balanced according to their signature.

3. **Thread Safety**: Each thread should have its own `qd_context`.

4. **Hot Reloading**: `qd_get_module` with a name already in use returns that same module, and
   `qd_add_script` appends to it, so rebuilding it with new source fails on the duplicate
   definitions (and leaves the module unbuilt). To reload, load the new source into a module with
   a fresh name (`ai_2`) and call that from then on, or build everything again in a new context.

## Common Patterns

### Returning Multiple Values

```c
// Native: ( -- x y z)
int get_vector(qd_context* ctx, void* userdata) {
    qd_push_f(ctx, 1.0);  // x
    qd_push_f(ctx, 2.0);  // y
    return qd_push_f(ctx, 3.0);  // z
}
```

### Passing Callbacks

A function value reaches a native as a counted struct, and `qd_args` hands the native its
reference. Call it with `qd_call_value(ctx, fn)` -- as often as needed -- and give the reference
back with `qd_struct_release(fn)` when it is no longer held. A `ptr` is raw memory and never a
function value, so the native's parameter is declared with the function type it takes.

<!-- doccheck: skip the host application registers these words; they cannot exist outside it -->
```quadrate
// Quadrate side
use engine

fn process() {
    10 def(x:i64 -- r:i64) { x 2 * } engine::with_callback print nl  // 40
}
```

```c
// C side: (x:i64 f:fn(i64 -- i64) -- r:i64), applies f twice
int with_callback(qd_context* ctx, void* userdata) {
    int64_t x;
    void* fn;
    qd_args(ctx, "engine::with_callback", "ip", &x, &fn);  // fn's reference is now ours

    qd_push_i(ctx, x);
    qd_call_value(ctx, fn);   // leaves f(x)
    qd_call_value(ctx, fn);   // leaves f(f(x))

    qd_struct_release(fn);    // done with it
    return QD_OK;
}

qd_register_function(engine, "with_callback", "(x:i64 f:fn(i64 -- i64) -- r:i64)",
                     with_callback, NULL);
```

`qd_struct_release` is declared in `<quadrate/rt/qd_struct.h>`.

Going the other way, a native can hand a script a function value made from a C function.
With a NULL environment the function is a plain `int fn(qd_context*)`:

```c
static int forty_two(qd_context* ctx) { return qd_push_i(ctx, 42); }

// Native: ( -- f:fn( -- i64))
int make_fn(qd_context* ctx, void* userdata) {
    void* fn = qd_closure_new((void*)forty_two, NULL, 0);
    return qd_push_struct(ctx, fn);   // hands the stack our reference
}
```

With an environment, `qd_closure_new(fn, env, captures)`, the function takes it as a second
argument: `int fn(qd_context* ctx, void* env)`.

### Error Propagation

A native that can fail is registered with a `!` signature, and scripts handle it like any other
fallible call. The C side has a fixed contract: on success, push the outputs and then the status
`1`; on failure, `return qd_fail(ctx, code, fmt, ...)`, which records the code (not 0) and the
message and pushes the code as the status. Push no outputs on failure: the caller gets zero
placeholders in their place.

```c
#include <inttypes.h>   // PRId64

// Native: (x:i64 -- r:i64)!
int checked_half(qd_context* ctx, void* userdata) {
    int64_t x;
    qd_args(ctx, "engine::checked_half", "i", &x);
    if (x % 2 != 0) {
        return qd_fail(ctx, 2, "engine::checked_half: %" PRId64 " is odd", x);
    }
    qd_push_i(ctx, x / 2);     // the output
    return qd_push_i(ctx, 1);  // then the status: success
}

qd_register_function(engine, "checked_half", "(x:i64 -- r:i64)!", checked_half, NULL);
```

<!-- doccheck: skip the host application registers these words; they cannot exist outside it -->
```quadrate
// Quadrate side
use engine

fn halve(x:i64 -- ) {
    engine::checked_half ok if {
        print nl
    } else {
        drop                  // the zero placeholder
        err -> code -> msg
        msg print nl          // engine::checked_half: 7 is odd
    }
}
```

The script sees the call's status as `Ok` (0) on success, even though the native pushed `1`.

## Language Bindings

Wrappers around the C embedding API are available for other languages:

- **Go**: [go-quadrate](https://github.com/quadrate-language/go-quadrate)
- **Python**: [python-quadrate](https://github.com/quadrate-language/python-quadrate)
