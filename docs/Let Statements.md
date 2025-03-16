# LET Statements in ForthJIT

## Overview of `LET`

The `LET` construct is an extension in ForthJIT that allows you to define infix numerical functions that calculate floating point values.

### **Key Features of `LET`**
1. **Constant Inputs**: The inputs to the function remain constant throughout its evaluation.
2. **Stepwise Logic**: Use `WHERE` statements to define intermediate values step-by-step before arriving at the final results.
3. **Modular Design**: `LET` enables breaking down a calculation into smaller, logical steps, improving readability and maintainability.

## Syntax of `LET`
### **Structure**
``` forth
: FunctionName 
    LET (result1, result2, ...) = FN(arg1, arg2, ...) = 
        expression, expression, ...
        WHERE intermediate1 = expression1
        WHERE intermediate2 = expression2
        ...
        WHERE result1 = expression3 ;
```
- **Function Name**: The name of the function being defined.
- **Results**: Output values (`result1`, `result2`, ...).
- **Inputs**: Original constants passed as arguments (`arg1`, `arg2`, ...).
- **Intermediate Values**: Declared in `WHERE` statements to build upon earlier results or inputs.
- **Logical Flow**: Each `WHERE` statements are evaluated in dependency order, but you cant have circular dependencies.

## Examples Using `LET`
Below are detailed examples to demonstrate the best practices for writing `LET` statements.

### Example 1: Calculating Area of a Circle
**Purpose**: Compute the area of a circle given the radius.

``` forth
: area
     LET (area) = FN(radius) = pi * pow( radius, 2 ) 
        WHERE pi = 3.141592653589793; 
```
**Usage**:
``` forth
1.0 area f.
```
**Output**:
`3.141592653589793`

### Example 2: Calculating Building Height
**Purpose**: Compute the height of a building given the shadow length and the angle of elevation in degrees.
``` forth
: building_height
     LET (height) = FN(shadowLength, angleDegrees) = shadowLength * tan(angleRadians)
      WHERE angleRadians = angleDegrees * (pi / 180)
      WHERE pi = 3.141592653589793 ;
```
**Usage**:
``` forth
10. 45. building_height f.
```
**Output**:
`10.0`
### Example 3: Calculating Sine Wave Position
**Purpose**: Compute the position of a sine wave given amplitude, frequency, and time.
``` forth
: sine_wave_p 
    LET (position) = FN(amplitude, frequency, time) = amplitude * sin(2 * pi * frequency * time)
       WHERE pi = 3.141592653589793 ;
```
**Usage**:
``` forth
3.0 2.0 0.125 sine_wave_p f.
```
**Output**:
`3.0`
### Example 4: Calculating Distance Between Two Points
**Purpose**: Compute the Euclidean distance between two points `(x1, y1)` and `(x2, y2)`.
``` forth
: point_distance
    LET (distance) = FN(x1, y1, x2, y2) = hypot(x2 - x1, y2 - y1) ;
```
**Usage**:
``` forth
100. 100. 200. 200. point_distance f.
```
**Output**:
`141.42`
### Example 5: Mandelbrot Iteration (`M2`)
**Purpose**: Iterate values for a step in the Mandelbrot set calculation to compute the next complex value `(z_next_re, z_next_im)` and its magnitude.
``` forth
: mbrot LET 
    (z_next_re, z_next_im, mag) = FN(z_re, z_im, x, y) =
    re, im, rmag
    WHERE re = (z_re * z_re) - (z_im * z_im) + x       
    WHERE im = (2 * z_re * z_im) + y                   
    WHERE rmag = (re * re) + (im * im) ;                
```
**Usage**:
``` forth
1. 1. 1. 1. mbrot f. f. f.
```
**Output**:
`1 3 10`

## Best Practices for Writing `LET` Statements
1. **Keep Definitions Modular**: Break the calculation into smaller, logical steps using `WHERE` statements to define intermediate values explicitly.
2. **Avoid Repetition**: Use intermediate values to avoid recalculating the same expression multiple times.
3. **Use Meaningful Names**: Use self-explanatory names for inputs, intermediate values, and results to improve readability.
4. **Validate Intermediate Steps**: Check that each `WHERE` statement produces expected results before combining them in subsequent calculations.

## Common "Gotchas" for `LET`
1. **Forgetting the Order of Evaluation**: `WHERE` statements are evaluated sequentially, so ensure all dependencies are declared before use.
2. **Confusing Intermediate Values with Inputs**: Inputs (`arg1`, `arg2`, etc.) are constants, and intermediate values must be declared explicitly using `WHERE` if you want to modify or extend calculations.
3. **Reusing Variable Names**: Avoid reusing names from inputs or prior variables unless you deliberately want to overwrite them in subsequent logic.
