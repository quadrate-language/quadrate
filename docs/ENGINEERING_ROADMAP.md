# Engineering Roadmap: From Toy to Professional Language

This document outlines the critical engineering phases required to elevate Quadrate from an experimental "toy" language to a professional-grade, production-ready system.

## Phase 1: Compiler Robustness (The "Brain" Upgrade)
**Target:** `lib/qc/src/semantic_validator_typecheck.cc`
**Objective:** Replace brittle stack simulation with formal Control Flow Graph (CFG) analysis.

### 1. The Problem
The current compiler validates code by "pretending" to run it in a monolithic loop. This makes handling complex control flow (nested loops, breaks, returns) error-prone and brittle.

### 2. The Solution: Stack Effect Analysis
Instead of simulating line-by-line, the compiler will:
1.  **Decompose** code into **Basic Blocks** (linear sequences with no jumps).
2.  **Calculate** the Net Stack Effect of each block (e.g., "Consumes 2, Produces 1").
3.  **Connect** blocks into a **Control Flow Graph (CFG)**.
4.  **Verify** the graph ensures stack consistency on all paths.

### 3. Implementation Plan
*   **Step 1:** Create `StackEffect getSignature(AstNode*)` to isolate instruction logic.
*   **Step 2:** Build `CfgBuilder` to traverse AST and generate `BasicBlock` graph nodes.
*   **Step 3:** Implement graph walker to verify stack depths merge correctly at join points (e.g., end of `if/else`).

---

## Phase 2: Runtime Scalability & Safety (The "Engine" Upgrade)
**Target:** `lib/qdrt` (Runtime Library)
**Objective:** Remove the global lock bottleneck and fix memory leaks caused by circular references.

### 1. Problem A: The Global Lock (Scalability Killer)
**Current State:** `lib/qdrt/src/ptr_registry.h` uses a single global mutex to protect a hash map of all valid pointers.
**Impact:** Multi-threaded applications execute serially when allocating or accessing objects.
**Solution: Sharded Registry or Thread-Local Heaps**

#### Implementation Instructions (Agent Task)
1.  **Modify `ptr_registry_t`:**
    *   Instead of `mutex_storage` (single lock), implement **Lock Sharding**.
    *   Create an array of 64 mutexes: `mutex_platform_t buckets_mutexes[64]`.
2.  **Update `ptr_registry_add` / `ptr_registry_contains`:**
    *   Calculate hash of the pointer: `h = hash(ptr)`.
    *   Determine shard index: `shard = h % 64`.
    *   Only lock `buckets_mutexes[shard]`.
    *   *Result:* 64 threads can now allocate/access objects simultaneously with minimal contention.
3.  **Alternative (Advanced):** Move to **Thread-Local Registries** (requires changing how objects are passed between threads). Stick to Sharding for immediate 10x performance gain with minimal refactoring.

### 2. Problem B: Memory Leaks (Safety Hazard)
**Current State:** `lib/qdrt/src/qd_struct.c` uses simple Reference Counting (`retain`/`release`).
**Impact:** Cycles (A -> B -> A) are never freed, leaking memory forever.
**Solution: Cycle-Collection or Weak References**

#### Implementation Instructions (Agent Task)
1.  **Add `Markable` Interface:**
    *   Add `void (*trace)(void* self, void* visitor)` to the vtable of all managed objects (Arrays, Structs).
    *   This function must call `visitor` on every managed object it holds references to.
2.  **Implement `CycleCollector`:**
    *   Create a global list of "Potential Cycle Roots" (objects that had their ref-count decremented but not to zero).
    *   Periodically (or on allocation pressure), run a **Mark-and-Sweep**:
        *   **Red** (candidate for collection).
        *   **Green** (reachable from stack/globals).
    *   Traverse from current Stack Roots. Mark everything reachable as Green.
    *   Any object in the "Potential Cycle Roots" list that remains Red is a leaked cycle -> Free it.

### 3. Summary of Phase 2 Goals
*   **Performance:** ~10x-50x speedup in multi-threaded workloads via Lock Sharding.
*   **Safety:** Zero memory leaks for long-running services via Cycle Collection.
