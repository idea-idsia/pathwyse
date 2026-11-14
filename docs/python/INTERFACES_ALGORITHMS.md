# Algorithms

Methods to configure, enable, disable and reset algorithms, and to run the solver.

---

## Setup

#### setupAlgorithms

```python
def setupAlgorithms(self)
```

Configures algorithms from `pathwyse.set`, or default settings if not found.

#### setAlgorithms

```python
def setAlgorithms(self, names, active=None)
```

Sets the algorithms to be used by the solver.

- **names** *(list[str])* — list of algorithm names.
- **active** *(list[bool], optional)* — enable flags, one per algorithm. All enabled by default if omitted.

#### setAlgorithmsParallel

```python
def setAlgorithmsParallel(self, parallel)
```

Sets whether algorithms should run in parallel.

- **parallel** *(bool)* — `True` to enable parallel execution.

---

## Enable / disable

#### enableAlgorithm

```python
def enableAlgorithm(self, id)
```

Enables the algorithm with the given id.

- **id** *(int)* — algorithm id.

#### disableAlgorithm

```python
def disableAlgorithm(self, id)
```

Disables the algorithm with the given id.

- **id** *(int)* — algorithm id.

#### enableAllAlgorithms

```python
def enableAllAlgorithms(self)
```

Enables all algorithms.

#### disableAllAlgorithms

```python
def disableAllAlgorithms(self)
```

Disables all algorithms.

---

## Reset

#### resetAlgorithm

```python
def resetAlgorithm(self, id, reset_level)
```

Resets the algorithm with the given id to an unoptimized state.

- **id** *(int)* — algorithm id.
- **reset_level** *(int)* — should be `0`; not used in PathWyse 1.1.

#### resetAllAlgorithms

```python
def resetAllAlgorithms(self, reset_level)
```

Resets all algorithms to an unoptimized state.

- **reset_level** *(int)* — should be `0`; not used in PathWyse 1.1.

---

## Query

#### isAlgorithmEnabled

```python
def isAlgorithmEnabled(self, id) -> bool
```

Returns `True` if the algorithm with the given id is currently enabled, `False` otherwise.

- **id** *(int)* — algorithm id.

#### getEnabledAlgorithms

```python
def getEnabledAlgorithms(self) -> list[int]
```

Returns the ids of all currently enabled algorithms.

#### areAlgorithmsParallel

```python
def areAlgorithmsParallel(self) -> bool
```

Returns `True` if algorithms are configured to run in parallel, `False` otherwise.

---

## Solve

#### solve

```python
def solve(self)
```

Solves the problem using the enabled algorithms.
