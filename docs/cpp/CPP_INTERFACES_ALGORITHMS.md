# Algorithms

Methods to configure, enable, disable and reset algorithms, and to run the solver.

---

## Setup

#### setupAlgorithms

```cpp
void setupAlgorithms()
```

Configures algorithms from `pathwyse.set`, or default settings if not found.

#### setAlgorithms

```cpp
void setAlgorithms(std::vector<std::string> names, std::vector<bool> active = {})
```

Sets the algorithms to be used by the solver.

- **names** *(std::vector\<std::string\>)* — list of algorithm names.
- **active** *(std::vector\<bool\>)* — enable flags, one per algorithm. All enabled by default if omitted.

#### addAlgorithm

```cpp
void addAlgorithm(std::string name)
void addAlgorithm(Algorithm& algorithm)
```

Appends an algorithm to the solver's list. The first overload creates the algorithm by name; the second registers an externally constructed `Algorithm` object.

- **name** *(std::string)* — algorithm name.
- **algorithm** *(Algorithm&)* — reference to a custom Algorithm instance.

#### changeAlgorithm

```cpp
void changeAlgorithm(int id, std::string name)
void changeAlgorithm(int id, Algorithm& algorithm)
```

Replaces the algorithm at position `id`. The first overload substitutes by name; the second substitutes with an external object.

- **id** *(int)* — algorithm id.
- **name** *(std::string)* — algorithm name.
- **algorithm** *(Algorithm&)* — reference to a custom Algorithm instance.

#### clearAlgorithms

```cpp
void clearAlgorithms()
```

Removes all algorithms from the solver.

#### setAlgorithmsParallel

```cpp
void setAlgorithmsParallel(bool parallel)
```

Sets whether algorithms should run in parallel.

- **parallel** *(bool)* — `true` to enable parallel execution.

---

## Enable / disable

#### enableAlgorithm

```cpp
void enableAlgorithm(int id)
```

Enables the algorithm with the given id.

- **id** *(int)* — algorithm id.

#### disableAlgorithm

```cpp
void disableAlgorithm(int id)
```

Disables the algorithm with the given id.

- **id** *(int)* — algorithm id.

#### enableAllAlgorithms

```cpp
void enableAllAlgorithms()
```

Enables all algorithms.

#### disableAllAlgorithms

```cpp
void disableAllAlgorithms()
```

Disables all algorithms.

---

## Reset

#### resetAlgorithm

```cpp
void resetAlgorithm(int id, int reset_level)
```

Resets the algorithm with the given id to an unoptimized state.

- **id** *(int)* — algorithm id.
- **reset_level** *(int)* — should be `0`; not used in PathWyse 1.1.

#### resetAllAlgorithms

```cpp
void resetAllAlgorithms(int reset_level)
```

Resets all algorithms to an unoptimized state.

- **reset_level** *(int)* — should be `0`; not used in PathWyse 1.1.

---

## Query

#### isAlgorithmEnabled

```cpp
bool isAlgorithmEnabled(int id)
```

Returns `true` if the algorithm with the given id is currently enabled.

- **id** *(int)* — algorithm id.

#### getEnabledAlgorithms

```cpp
std::vector<int> getEnabledAlgorithms()
```

Returns the ids of all currently enabled algorithms.

#### areAlgorithmsParallel

```cpp
bool areAlgorithmsParallel()
```

Returns `true` if algorithms are configured to run in parallel.

#### getAlgorithm

```cpp
Algorithm* getAlgorithm(int id)
```

Returns a pointer to the algorithm with the given id.

- **id** *(int)* — algorithm id.

---

## Solve

#### solve

```cpp
void solve()
```

Solves the problem using the enabled algorithms.
