attribute vec4 position;
attribute vec4 color;
attribute vec2 texcoord;
        
// output to fragment shader
varying vec4 v_color;
varying vec2 v_texcoord;
        
// custom input from program
uniform mat4 ProjectionMatrix;

void main()
{
    gl_Position = ProjectionMatrix * position;
    v_color = color;
    v_texcoord = texcoord;
}
