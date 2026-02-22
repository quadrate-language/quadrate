# Embedding Quadrate

This guide shows how to embed Quadrate as a scripting language in your C/C++ applications. Quadrate is well-suited for embedding because it:

- Compiles to native code (fast execution)
- Has a simple stack-based model (easy to interface with)
- Provides a clean C API
- Supports registering native functions

## Quick Start

Here's the minimal code to embed Quadrate:

```c
#include <qd/qd.h>

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
gcc -o myapp myapp.c -lqd -lqdrt -L/path/to/quadrate/dist/lib -I/path/to/quadrate/dist/include
```

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
qd_add_script(mod, "fn square(x:i64 -- result:i64) { dup * }");
qd_add_script(mod, "fn cube(x:i64 -- result:i64) { dup dup * * }");
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

Native functions take a `qd_context*` and a `void* userdata`, and return `int`:

```c
#include <qd/qd.h>
#include <qdrt/runtime.h>

// Native function: pushes current time onto stack
int native_get_time(qd_context* ctx, void* userdata) {
    time_t now = time(NULL);
    return qd_push_i(ctx, (int64_t)now);
}

int main(void) {
    qd_context* ctx = qd_create_context(1024);
    qd_module* utils = qd_get_module(ctx, "utils");

    // Register the native function
    qd_register_function(utils, "get_time", native_get_time, NULL);

    qd_build(utils);

    // Call from Quadrate
    qd_execute(ctx, "utils::get_time print nl");

    qd_free_context(ctx);
    return 0;
}
```

### Reading Arguments from the Stack

Use `qd_stack_pop` to read arguments:

```c
#include <qdrt/stack.h>

// Native function: (a b -- sum)
int native_add(qd_context* ctx, void* userdata) {
    qd_stack_element_t b, a;

    // Pop in reverse order (b is on top)
    qd_stack_pop(ctx->st, &b);
    qd_stack_pop(ctx->st, &a);

    int64_t sum = a.value.i + b.value.i;
    return qd_push_i(ctx, sum);
}
```

### Stack Element Types

The `qd_stack_element_t` structure:

```c
typedef struct {
    qd_stack_type type;  // QD_STACK_TYPE_INT, _FLOAT, _STR, _PTR
    union {
        int64_t i;       // Integer value
        double f;        // Float value
        qd_string_t* s;  // String value (reference counted)
        void* p;         // Pointer value
    } value;
} qd_stack_element_t;
```

### Push Functions

```c
qd_push_i(ctx, 42);           // Push integer
qd_push_f(ctx, 3.14159);      // Push float
qd_push_s(ctx, "hello");      // Push string (copied)
qd_push_p(ctx, my_pointer);   // Push pointer
```

### Userdata

Each native function receives a `void* userdata` pointer that was passed at registration time. This lets callbacks access application state without globals:

```c
typedef struct { int score; const char* name; } Player;

int get_score(qd_context* ctx, void* userdata) {
    Player* p = (Player*)userdata;
    return qd_push_i(ctx, p->score);
}

Player player = { .score = 42, .name = "Alice" };
qd_register_function(mod, "get_score", get_score, &player);
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
qd_register_function(sensors, "temperature", read_temp, &hw);
qd_register_function(sensors, "pressure", read_pressure, &hw);
// No qd_add_script or qd_build needed for native-only modules

// Another module can use it normally
qd_module* app = qd_get_module(ctx, "app");
qd_add_script(app, "use sensors\nfn check() { sensors::temperature print nl }");
qd_build(app);
```

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
    printf("[Engine] Spawned entity %lld at (%.1f, %.1f)\n", e->id, x, y);
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
    printf("[Engine] Entity %lld took %lld damage (health: %lld)\n",
           id, amount, e->health);

    if (e->health <= 0) {
        e->active = false;
        printf("[Engine] Entity %lld destroyed!\n", id);
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
#include <qd/qd.h>
#include <qdrt/runtime.h>
#include <qdrt/stack.h>
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
    qd_stack_element_t id_elem;
    qd_stack_pop(ctx->st, &id_elem);

    Entity* e = engine_get_entity(game, id_elem.value.i);
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
    qd_stack_element_t id_elem;
    qd_stack_pop(ctx->st, &id_elem);

    Entity* e = engine_get_entity(game, id_elem.value.i);
    return qd_push_i(ctx, e ? e->health : 0);
}

// Move entity (id dx dy --)
int script_move(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    qd_stack_element_t dy_elem, dx_elem, id_elem;
    qd_stack_pop(ctx->st, &dy_elem);
    qd_stack_pop(ctx->st, &dx_elem);
    qd_stack_pop(ctx->st, &id_elem);

    engine_move(game, id_elem.value.i, dx_elem.value.f, dy_elem.value.f);
    return QD_OK;
}

// Damage entity (id amount --)
int script_damage(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    qd_stack_element_t amount_elem, id_elem;
    qd_stack_pop(ctx->st, &amount_elem);
    qd_stack_pop(ctx->st, &id_elem);

    engine_damage(game, id_elem.value.i, amount_elem.value.i);
    return QD_OK;
}

// Spawn new entity (x y health -- id)
int script_spawn(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    qd_stack_element_t health_elem, y_elem, x_elem;
    qd_stack_pop(ctx->st, &health_elem);
    qd_stack_pop(ctx->st, &y_elem);
    qd_stack_pop(ctx->st, &x_elem);

    Entity* e = engine_spawn(game, x_elem.value.f, y_elem.value.f,
                             health_elem.value.i, NULL);
    return qd_push_i(ctx, e ? e->id : -1);
}

// Get delta time ( -- dt)
int script_delta_time(qd_context* ctx, void* userdata) {
    GameState* game = (GameState*)userdata;
    return qd_push_f(ctx, game->delta_time);
}

// Calculate distance between two points (x1 y1 x2 y2 -- dist)
int script_distance(qd_context* ctx, void* userdata) {
    qd_stack_element_t y2, x2, y1, x1;
    qd_stack_pop(ctx->st, &y2);
    qd_stack_pop(ctx->st, &x2);
    qd_stack_pop(ctx->st, &y1);
    qd_stack_pop(ctx->st, &x1);

    double dx = x2.value.f - x1.value.f;
    double dy = y2.value.f - y1.value.f;
    return qd_push_f(ctx, sqrt(dx*dx + dy*dy));
}

// Log a message (msg --)
int script_log(qd_context* ctx, void* userdata) {
    qd_stack_element_t msg;
    qd_stack_pop(ctx->st, &msg);

    if (msg.type == QD_STACK_TYPE_STR) {
        printf("[Script] %s\n", qd_string_data(msg.value.s));
        qd_string_release(msg.value.s);  // Release reference
    }
    return QD_OK;
}

// === Script system initialization ===

void scripting_init(GameState* game) {
    script_ctx = qd_create_context(4096);

    // Create the 'engine' module — pass game state as userdata to each function
    qd_module* engine = qd_get_module(script_ctx, "engine");

    qd_register_function(engine, "self", script_self, game);
    qd_register_function(engine, "get_pos", script_get_pos, game);
    qd_register_function(engine, "get_health", script_get_health, game);
    qd_register_function(engine, "move", script_move, game);
    qd_register_function(engine, "damage", script_damage, game);
    qd_register_function(engine, "spawn", script_spawn, game);
    qd_register_function(engine, "delta_time", script_delta_time, game);
    qd_register_function(engine, "distance", script_distance, game);
    qd_register_function(engine, "log", script_log, game);

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

```quadrate
// scripts/player.qd
// Player behavior script

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

fn take_damage(amount:i64 -- ) {
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

```quadrate
// scripts/enemy.qd
// Enemy AI script

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
| `qd_register_function(mod, name, fn, userdata)` | Register native function |
| `qd_build(mod)` | Compile the module |
| `qd_is_compiled(mod)` | Check if compilation succeeded |

### Execution

| Function | Description |
|----------|-------------|
| `qd_execute(ctx, expr)` | Execute Quadrate expression |

### Stack Operations

| Function | Description |
|----------|-------------|
| `qd_push_i(ctx, val)` | Push integer |
| `qd_push_f(ctx, val)` | Push float |
| `qd_push_s(ctx, str)` | Push string (copied) |
| `qd_push_p(ctx, ptr)` | Push pointer |
| `qd_stack_pop(ctx->st, &elem)` | Pop element |
| `qd_stack_peek(ctx->st, &elem)` | Peek top element |

### Stack Element Access

```c
qd_stack_element_t elem;
qd_stack_pop(ctx->st, &elem);

switch (elem.type) {
    case QD_STACK_TYPE_INT:
        printf("Integer: %lld\n", elem.value.i);
        break;
    case QD_STACK_TYPE_FLOAT:
        printf("Float: %f\n", elem.value.f);
        break;
    case QD_STACK_TYPE_STR:
        printf("String: %s\n", qd_string_data(elem.value.s));
        qd_string_release(elem.value.s);  // Don't forget!
        break;
    case QD_STACK_TYPE_PTR:
        printf("Pointer: %p\n", elem.value.p);
        break;
}
```

## Best Practices

1. **Error Handling**: Always check `qd_is_compiled()` after building modules.

2. **String Memory**: When popping strings, call `qd_string_release()` when done.

3. **Stack Balance**: Native functions must leave the stack balanced according to their signature.

4. **Thread Safety**: Each thread should have its own `qd_context`.

5. **Hot Reloading**: To reload scripts, create a new module with the same name and rebuild.

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

```quadrate
// Quadrate side
fn my_callback(x:i64 -- result:i64) { x 2 * }
fn process() {
    10 fn(x:i64 -- r:i64) { x 2 * } engine::with_callback
}
```

### Error Propagation

```c
int native_might_fail(qd_context* ctx, void* userdata) {
    if (error_condition) {
        // Set error state
        ctx->error_code = 1;
        return -1;
    }
    return qd_push_i(ctx, result);
}
```
