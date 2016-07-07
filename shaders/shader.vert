#version 140
//#version 330 core

in vec3 vertex_position;
in vec3 vertex_normal;
in vec3 vertex_color;

out vec3 frag_position;
out vec3 frag_color;
out vec3 frag_normal;

uniform mat4 MVP;

void main(){
  gl_Position = MVP * vec4(vertex_position, 1);
  frag_position = vertex_position;
  frag_color    = vertex_color;
  frag_normal   = vertex_normal;
}


