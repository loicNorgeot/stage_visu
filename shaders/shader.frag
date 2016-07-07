#version 140
//#version 330 core

in vec3 frag_position;
in vec3 frag_color;
in vec3 frag_normal;

//Structure options:
// 0 - filled
// 1 - filled with wireframe
// 2 - wireframe only
// 3 - points only
uniform int uStructure;
uniform int uSecondPass;

//Lights options:
// 0 - no shading
// 1 - flat shading
// 2 - smooth shading
uniform int uLighting;

//Colors options:
// 0 - brute color
// 1 - .sol color
// 2 - normal colors
// 3 - checker
uniform int uColor;

//picking rendering
uniform int picking;

//clipping
uniform int clipping;

uniform mat4 MVP;
uniform mat4 M;
uniform mat4 V;
uniform vec3 objectColor;

out vec3 out_color;

vec3 light(mat4 light_matrix, vec3 mater_color, int light);

void main(){
  if(clipping==1){
    if(frag_position.x<-0.001 || frag_position.x>0.001){
      discard;
    }
    else{
      out_color = vec3(1,1,1);
      return;
    }
  }
  vec3 temp_color = vec3(1,1,1);

  mat4 LM = mat4(
  vec4(-2,2,10,0),
  vec4(1,1,1,0),
  vec4(0.15,0.75,0.1,0),
  vec4(120,12.0,1,0)
  );

  mat4 back = mat4(
  vec4(0,0,-10,0),
  vec4(1,1,1,0),
  vec4(0,0.85,0.15,0),
  vec4(120,12.0,1,0)
  );

  if(uColor == 0)
    temp_color = objectColor;
  else if(uColor == 1)
    temp_color = frag_color;
  else if(uColor == 2)
    temp_color = vec3( abs(frag_normal.x), abs(frag_normal.y), abs(frag_normal.z) );

  if(uStructure == 1 && uSecondPass==0)
    temp_color = objectColor;

  //if(uLighting == 0)
  if(uLighting == 1)
    temp_color = light(LM, temp_color, 0) + light(back, temp_color, 0);
  else if(uLighting == 2)
    temp_color = light(LM, temp_color, 1) + light(back, temp_color, 1);

  if(picking == 1)
    out_color = objectColor;
  else
    out_color = temp_color;

  if (uColor == 3){
    if( (int(20*frag_position.x+20) + int(20*frag_position.z+20) + int(20*frag_position.y+20) ) %2 < 1)
      out_color = vec3(0,0,0);
    else
      out_color = vec3(1,1,1);
  }

}
