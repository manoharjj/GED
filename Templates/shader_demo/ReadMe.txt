Simple Shader Demo
~~~~~~~~~~~~~~~~~~

This demo renders a single triangle to demonstrate the usage of SV_VertexID and
simple vertex / pixel shader operations.

All passes of the technique "Render" in "demoEffect.fx" are automatically added
to the combo box in the GUI to allow for interactive shader editing. To update
the displayed passes, compile the shader (Ctrl + F7) while your program is running
WITHOUT debugger attached (Start using Ctrl + F5) and press the "Reload Shader"
Button (or just hit F5).

Four passes are available at the start:

* Simple: Draws the triangle with fixed color and fixed vertex positions
* Color: Assigns a color to each vertex to demonstrate interpolation
* SineMovement: Modifies each vertex' y position with a sine curve
* SineColor: Modifies each pixels color in the pixel shader with a sine curve 