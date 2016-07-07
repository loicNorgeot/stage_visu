// General libraries including
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <set>
#include <iostream>
#include <fstream>

// OpenGL libraries
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// glm headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/vector_angle.hpp>

// Freetype for fonts rendering
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H

//libmesh from the "Commons" library
extern "C" {
#include <libmesh5.h>
}


// ************************************
// ************************************
// FUNCTIONS PROTOTYPES AND UTILITIES

// ************************************
// Environment initialization
// glfw for windows and input
void initGLFW();
// glew for openGL extensions
void initGLEW();

// ************************************
// OpenGL custom wrappers for buffer operations
template<typename T> GLuint createBuffer(GLenum target, std::vector<T> *data);
GLuint createVAO();
void bindBuffer(GLenum target, GLuint buffer, int ID, int attrib=0, char* name=nullptr);
template<typename T> void updateBuffer(GLuint pBuffer, std::vector<T> *data);
void send(int ID, glm::mat4 &m, char* name);
void send(int ID, glm::vec3 v, char* name);
void send(int ID, float f, char* name);
void send(int ID, int i, char* name);

// ************************************
// Shaders loading and compilation
std::string readCode(std::string path);
GLuint compileShader(GLenum target, std::string code);
GLuint compileProgram(GLuint vID, GLuint fID);
GLuint loadProgram(std::string vertex_file_path, std::string fragment_file_path, std::string functions_file_path);

// ************************************
// Classes for text rendering
struct Character {
    GLuint TextureID;    // ID handle of the glyph texture
    glm::ivec2 Size;     // Size of glyph
    glm::ivec2 Bearing;  // Offset from baseline to left/top of glyph
    GLuint Advance;      // Horizontal offset to advance to next glyph
};
class GUI{
public:
  GLuint VAO, VBO, fontID;
  std::map<GLchar, Character> Characters;
  GUI(std::string vertPath, std::string fragPath, std::string font);
  void text(std::string text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color);
};

// ************************************
// Custom object and context classes, with pointers (very important)
// Custom context class
class Context{
public:
  int w,h;
  float fov, zmin, zmax, zoom;
  glm::mat4 VIEW, PROJ;
  glm::vec3 cam, look, up;
  void update(){
    cam  = zoom * glm::normalize(cam);
    VIEW = glm::lookAt(cam, look, up);
    PROJ = glm::perspective(glm::radians(fov), (float)w / (float)h, zmin, zmax);
  }
};
Context* myContext;
// Custom object class
class Object{
public:
  std::vector<glm::vec3>            vertices, colors;
  std::vector<int>                  triangles;
  std::vector<std::vector<int>>     neighbours;
  std::vector<int>                  selected;
  glm::mat4                         MODEL;
  GLuint VAO, vBuffer, cBuffer, iBuffer, nBuffer, cPickingBuffer;
  void read(char * mesh_path);

  void createNeighbours();//A créer et remplir à la lecture de l'objet
  std::vector<int> getNeighbours(int ind, int level);


};
Object* myObject;

// ************************************
// Ray and intersection computing
glm::vec3 computeRay(Context* c, int x, int y){
  int   w    = c->w;
  int   h    = c->h;
  glm::vec2 norm_pos(((float)x/((float)w*0.5f) - 1) * ((float)w/(float)h), 1.0f - (float)y/((float)h*0.5f));

  glm::vec2 fov_coordinates = tanf(glm::radians(c->fov) * 0.5f) * norm_pos;

  glm::vec3 near_point(fov_coordinates.x * c->zmin, fov_coordinates.y * c->zmin, -c->zmin);
  glm::vec3 far_point( fov_coordinates.x * c->zmax, fov_coordinates.y * c->zmax, -c->zmax);

  glm::mat4 inv             = glm::inverse(c->VIEW);
  near_point                = c->cam + glm::vec3( inv * glm::vec4(near_point, 0) );
  far_point                 = c->cam + glm::vec3( inv * glm::vec4(far_point,  0) );

  return glm::normalize(far_point - near_point);
}
bool intersectsWithTriangle(Context* c, Object* o, int x, int y, int& ind){

  bool intersects = false;
  std::vector<glm::vec3> intersections;
  std::vector<int> intersectionsIndices;

  for(int i = 0 ; i < o->triangles.size() ; i+=3){
    glm::vec3 v1 = glm::vec3( o->MODEL * glm::vec4( o->vertices[o->triangles[i]],   1) );
    glm::vec3 v2 = glm::vec3( o->MODEL * glm::vec4( o->vertices[o->triangles[i+1]], 1) );
    glm::vec3 v3 = glm::vec3( o->MODEL * glm::vec4( o->vertices[o->triangles[i+2]], 1) );
    glm::vec3 ray = computeRay(c, x, y);
    glm::vec3 tmpIntersection;
    if( glm::intersectRayTriangle(c->cam, ray, v1, v2, v3, tmpIntersection) ){
      intersections.push_back(c->cam + ray*float(tmpIntersection.z));
      intersectionsIndices.push_back(i);
      intersects=true;
    }
  }

  glm::vec3 intersection(1000);
  for(int i = 0 ; i < intersections.size() ; i++){
    if(glm::distance(intersections[i], c->cam) < glm::distance(intersection, c->cam)){
      intersection = intersections[i];
      ind          = intersectionsIndices[i];
    }
  }
  std::cout << intersection.x << " " << intersection.y << " " << intersection.z << std::endl;
  return intersects;
}


// ************************************
// ************************************
// Callbacks functions, used for user input

double lastX, lastY;
int lastState = GLFW_RELEASE;
int rayon = 15;
bool add = true;
static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos){

    //}
    // If left button is pressed, compute the intersection of the mouse and the object
    if ( GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) && glm::distance(glm::vec2(xpos,ypos), glm::vec2(lastX, lastY)) > 20){
        lastX = xpos;
        lastY = ypos;
      int indice = -1;

      // Does the ray intersects? If so, indice is the index of the triangle intersected.
      bool intersects = intersectsWithTriangle(myContext, myObject, xpos, ypos, indice);

      // If intersection, paint everything white except the concerned triangle
      if(intersects){

      /* Old version
        for(int i = 0 ; i < myObject->colors.size() ; i++)
          myObject->colors[ i ] = glm::vec3(1);
        myObject->colors[ myObject->triangles[indice] ] = glm::vec3(1,0.5,0);
        myObject->colors[ myObject->triangles[indice+1] ] = glm::vec3(1,0.5,0);
        myObject->colors[ myObject->triangles[indice+2] ] = glm::vec3(1,0.5,0);

        //for(int k; k < myObject->neighbours[indice/3].size(); k++){
        for(int k; k < myObject->neighbours[indice/3].size(); k++){
            myObject->colors[ myObject->triangles[myObject->neighbours[indice/3][k]] ] = glm::vec3(1, 0.5, 0);
            myObject->colors[ myObject->triangles[myObject->neighbours[indice/3][k]+ 1]] = glm::vec3(1, 0.5, 0);
            myObject->colors[ myObject->triangles[myObject->neighbours[indice/3][k]+ 2]] = glm::vec3(1, 0.5, 0);
        }
        */

        //New version
        std::vector<int> neigh = myObject->getNeighbours(indice, rayon);
        //for(int n : neigh){
        //    std::cout << n << " ";
        //}
        //std::cout << std::endl;
        glm::vec3 color = add ? glm::vec3(1, 0.5, 0) : glm::vec3(1, 1, 1);
        for(int k; k < neigh.size(); k++){
            myObject->colors[ myObject->triangles[neigh[k] + 0]] = color;
            myObject->colors[ myObject->triangles[neigh[k] + 1]] = color;
            myObject->colors[ myObject->triangles[neigh[k] + 2]] = color;
        }

        updateBuffer( myObject->cBuffer, &myObject->colors);
      }
      // Else, paint everything white
      else{
        for(int i = 0 ; i < myObject->colors.size() ; i++)
          myObject->colors[ i ] = glm::vec3(1);
        updateBuffer( myObject->cBuffer, &myObject->colors);
      }
    }
}
void window_size_callback(GLFWwindow* window, int width, int height){
  myContext->w = width;
  myContext->h = height;
  myContext->update();
  glViewport(0, 0, width, height);
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    //xoffset is used for trackpads, only yoffset is of interest for us.
    //
    if(yoffset > 0 && myContext->fov <90){
      myContext->zoom *= 1+0.05;
      myContext->update();
    }
    if(yoffset < 0 && myContext->fov >30){
      myContext->zoom *= 1-0.05;
      myContext->update();
    }
}
void error_callback(int error, const char* description){
  fputs(description, stderr);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
  if(action == GLFW_PRESS){
    switch(key){
      case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GL_TRUE);
        break;
      case GLFW_KEY_TAB:
        add = !add;
        break;
      case GLFW_KEY_UP:
        rayon+=1;
        break;
      case GLFW_KEY_DOWN:
        rayon-=1;
        break;
    }
  }
  std::cout << rayon << std::endl;
}



// ************************************
// ************************************
// MAIN PROGRAM
int main(int argc, char** argv){

  // Initialization of object and context pointers
  myContext = new Context();
  myObject  = new Object();

  // GLFW and GLEW context and window creation
  initGLFW();
  GLFWwindow* w;
  glfwWindowHint( GLFW_SAMPLES, 4);
  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  //glfwWindowHint( GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);
  myContext->w = 640;
  myContext->h = 480;
  w = glfwCreateWindow(myContext->w,myContext->h,"TEST", NULL, NULL);
  glfwMakeContextCurrent(w);
  glfwSetInputMode(w, GLFW_STICKY_KEYS, GL_TRUE);
  if(w==NULL){
    std::cout << "Can't create GLFW window" << std::endl;
    glfwTerminate();
    exit(-1);
  }
  initGLEW();

  // Callbacks used for user input (linked with the above callback functions)
  glfwSetKeyCallback(w, key_callback);
  glfwSetErrorCallback(error_callback);
  glfwSetCursorPosCallback(w, cursor_pos_callback);
  glfwSetWindowSizeCallback(w, window_size_callback);
  glfwSetScrollCallback(w, scroll_callback);
  //glfwSetMouseButtonCallback(w, mouse_button_callback);

  // Shaders and text initialization
  std::string path    = "/home/him/dev/ogl/";
  std::string shaders = path + "shaders/";
  std::string fonts   = path + "fonts/";
  int ID   = loadProgram(shaders+"shader.vert", shaders+"shader.frag", shaders+"shader.functions");
  GUI* gui = new GUI(    shaders+"text.vert",   shaders+"text.frag",   fonts+"arial.ttf");

  // Objet creation
  myObject->read("/home/him/dev/ogl/257.o.mesh");
  myObject->colors.resize(myObject->vertices.size());
  for(int i = 0 ; i < myObject->vertices.size() ; i++)
    myObject->vertices[i]=5.0f*myObject->vertices[i];
  for(int i = 0 ; i < myObject->colors.size() ; i++){
    myObject->colors[i] = glm::vec3(1);
  }
  myObject->MODEL  = glm::mat4(1);

  // Buffer creation
  myObject->VAO = createVAO();
  myObject->vBuffer = createBuffer(GL_ARRAY_BUFFER, &myObject->vertices);
  myObject->cBuffer = createBuffer(GL_ARRAY_BUFFER, &myObject->colors);
  myObject->iBuffer = createBuffer(GL_ELEMENT_ARRAY_BUFFER, &myObject->triangles);
  myObject->nBuffer=0;

  // Buffers linking
  glUseProgram(ID);
  glBindVertexArray(myObject->VAO);
  bindBuffer(GL_ARRAY_BUFFER, myObject->vBuffer, ID, 0, "vertex_position");
  bindBuffer(GL_ARRAY_BUFFER, myObject->nBuffer, ID, 1, "vertex_normal");
  bindBuffer(GL_ARRAY_BUFFER, myObject->cBuffer, ID, 2, "vertex_color");
  bindBuffer(GL_ELEMENT_ARRAY_BUFFER, myObject->iBuffer, ID);
  // Link with 0 to reinitialize
  glBindVertexArray(0);
  glUseProgram(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // View parameters
  myContext->zoom  = 1.0f;
  myContext->up    = glm::vec3(0,1,0);
  myContext->cam   = glm::vec3(1,1,1);
  myContext->look  = glm::vec3(0,0,0);
  myContext->fov   = 70.0f;
  myContext->zmin  = 0.00005f;
  myContext->zmax  = 10.0f;
  myContext->update();

  myObject->createNeighbours();

  // Main display loop (executed every frame)
  while( ! (glfwGetKey(w,GLFW_KEY_ESCAPE)==GLFW_PRESS||glfwWindowShouldClose(w)==1) ){

    // Listen for input
    glfwPollEvents();

    // Clear the background
    glClearColor(0.1,0.1,0.1,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // OpenGL initialization
    glUseProgram(ID);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0,1.0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glPolygonMode(GL_BACK, GL_FILL);

    // MVP matrice and shader parameters (not that important)
    glm::mat4 MVP = myContext->PROJ * myContext->VIEW * myObject->MODEL;
    send(ID, MVP, 	"MVP");
    send(ID, myObject->MODEL, "M");
    send(ID, myContext->VIEW, "V");
    send(ID, 1, "uLighting");
    send(ID, 1,   "uColor");
    send(ID, 0,"uStructure");
    send(ID, glm::vec3(1,1,1), "objectColor");
    send(ID, 0, "uSecondPass");
    send(ID, 0, "picking");
    send(ID, 0,"clipping");

    // Bind the buffers to prepare drawing
    glBindVertexArray(myObject->VAO);
    bindBuffer(GL_ARRAY_BUFFER, myObject->cBuffer, ID, 2, "vertex_color");
    // DRAW THE OBJECT !!!!!
    glDrawElements(GL_TRIANGLES, myObject->triangles.size(), GL_UNSIGNED_INT, (void*)0);

    /*
    // If left button is pressed, compute the intersection of the mouse and the object
    if ( GLFW_PRESS == glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_1) ){
      int indice = -1;
      double x,y;
      glfwGetCursorPos( w, &x, &y);
      // Does the ray intersects? If so, indice is the index of the triangle intersected.
      bool intersects = intersectsWithTriangle(myContext, myObject, x, y, indice);

      // If intersection, paint everything white except the concerned triangle
      if(intersects){
        for(int i = 0 ; i < myObject->colors.size() ; i++)
          myObject->colors[ i ] = glm::vec3(1);
        myObject->colors[ myObject->triangles[indice] ] = glm::vec3(1,0.5,0);
        myObject->colors[ myObject->triangles[indice+1] ] = glm::vec3(1,0.5,0);
        myObject->colors[ myObject->triangles[indice+2] ] = glm::vec3(1,0.5,0);
        updateBuffer( myObject->cBuffer, &myObject->colors);
      }
      // Else, paint everything white
      else{
        for(int i = 0 ; i < myObject->colors.size() ; i++)
          myObject->colors[ i ] = glm::vec3(1);
        updateBuffer( myObject->cBuffer, &myObject->colors);
      }
    }
    */

    //Print the radius
    gui->text(std::to_string(rayon), 20.0f, 20.0f, 1, glm::vec3(1,0,0));
    std::string status = add ? "Addition" : "Substraction" ;
    gui->text(status, 20.0f, 60.0f, 1, glm::vec3(1,0,0));

    // Clean up at the end of a loop
    glBindVertexArray(0);
    glfwSwapBuffers(w);
    glfwSwapInterval(1);
  }

  // End the program
  glfwDestroyWindow(w);
  glfwTerminate();
  return 0;
}



// ****************************************
// Functions bodies

void initGLFW(){
  if(!glfwInit()){
    fprintf(stderr, "Failed to initialize GLFW\n");
    exit(-1);
  }
}
void initGLEW(){
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (GLEW_OK != err){
    std::cout << "GLEW_ERROR!" << std::endl;
    fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
    exit(-1);
  }
}

template<typename T>
GLuint createBuffer(GLenum target, std::vector<T> *data){
  if(data->size()==0)
    return 0;
  GLuint b;
  glGenBuffers( 1, &b);
  glBindBuffer( target, b);
  glBufferData( target, sizeof(T) * data->size(), &(*data)[0], GL_STATIC_DRAW);
  return b;
}
GLuint createVAO(){
  GLuint v;
  glGenVertexArrays(1, &v);
  glBindVertexArray(v);
  return v;
}
void bindBuffer(GLenum target, GLuint buffer, int ID, int attrib, char* name){
  if (target == GL_ELEMENT_ARRAY_BUFFER)
    glBindBuffer( target, buffer);
  else if(buffer!=0){
    glBindBuffer( target, buffer);
    glEnableVertexAttribArray( attrib );
    glVertexAttribPointer( attrib, 3, GL_FLOAT, GL_FALSE, 0, ( void*)0);
    glBindAttribLocation(ID, attrib, name);
  }
}
template<typename T>
void updateBuffer(GLuint pBuffer, std::vector<T> *data){
  glBindBuffer( GL_ARRAY_BUFFER, pBuffer);
  glBufferData( GL_ARRAY_BUFFER, sizeof(T) * data->size(), &(*data)[0], GL_STREAM_DRAW);
  glBindBuffer( GL_ARRAY_BUFFER, 0);
}
void send(int ID, glm::mat4 &m, char* name){
  int id = glGetUniformLocation(ID, name);
  glUniformMatrix4fv(id, 1, GL_FALSE, &m[0][0]);
}
void send(int ID, glm::vec3 v, char* name){
  int id = glGetUniformLocation(ID, name);
  glUniform3f(id, v.x, v.y, v.z);
}
void send(int ID, float f, char* name){
  int id = glGetUniformLocation(ID, name);
  glUniform1f(id, f);
}
void send(int ID, int i, char* name){
  int id = glGetUniformLocation(ID, name);
  glUniform1i(id, i);
}

std::string readCode(std::string path){
  std::string code = "";
  std::ifstream stream(path, std::ios::in);
  if(stream.is_open()){
    std::string Line = "";
    while(getline(stream, Line))
      code += "\n" + Line;
    stream.close();
  }
  else{
    std::cout << "Impossible to open " << path << std::endl;
    exit(-1);
  }
  return code;
}
GLuint compileShader(GLenum target, std::string code){
  GLuint ID = glCreateShader(target);
  GLint Result = GL_FALSE;
  int InfoLogLength=0;
  char const * pSrc = code.c_str();
  glShaderSource(ID, 1, &pSrc , NULL);
  glCompileShader(ID);
  glGetShaderiv(ID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(ID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if ( InfoLogLength > 1 ){
    std::vector<char> msg(InfoLogLength+1);
    glGetShaderInfoLog(ID, InfoLogLength, NULL, &msg[0]);
    printf("%s\n", &msg[0]);
    exit(-1);
  }
  return ID;
}
GLuint compileProgram(GLuint vID, GLuint fID){
  GLint  Result        = GL_FALSE;
  int    InfoLogLength = 0;
  GLuint ID            = glCreateProgram();

  glAttachShader(ID, vID);
  glAttachShader(ID, fID);
  glLinkProgram( ID);

  glGetProgramiv(ID, GL_LINK_STATUS, &Result);
  glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if ( InfoLogLength > 1 ){
    std::vector<char> ProgramErrorMessage(InfoLogLength+1);
    glGetProgramInfoLog(ID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
    printf("%s\n", &ProgramErrorMessage[0]);
    exit(-1);
  }
  std::cout << "Succesfully linked the shader program" << std::endl;

  glDetachShader(ID, vID);
  glDetachShader(ID, fID);
  glDeleteShader(vID);
  glDeleteShader(fID);

  return ID;
}
GLuint loadProgram(std::string vertex_file_path, std::string fragment_file_path, std::string functions_file_path){
  //Read the codes
  std::string vertCode, fragCode, funcCode;
  vertCode = readCode(vertex_file_path);
  fragCode = readCode(fragment_file_path);
  if(functions_file_path != "")
    funcCode = readCode(functions_file_path);

  //Compile the shaders
  GLuint vertID = compileShader(GL_VERTEX_SHADER, vertCode);
  GLuint fragID = compileShader(GL_FRAGMENT_SHADER, fragCode + funcCode);
  GLuint progID = compileProgram(vertID, fragID);

  return progID;
}


GUI::GUI(std::string vertPath, std::string fragPath, std::string font){
  fontID = loadProgram(vertPath, fragPath, "");
  glUseProgram(fontID);

  FT_Library ft;
  if (FT_Init_FreeType(&ft))
    std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
  FT_Face face;
  if (FT_New_Face(ft, font.c_str(), 0, &face))
    std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
  FT_Set_Pixel_Sizes(face, 0, 48);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (GLubyte c = 0; c < 128; c++){
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)){
      std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
      continue;
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RED,face->glyph->bitmap.width,face->glyph->bitmap.rows,0,GL_RED,GL_UNSIGNED_BYTE,face->glyph->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Character character = {texture,
      glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
      glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
      (GLuint)face->glyph->advance.x};
    Characters.insert(std::pair<GLchar, Character>(c, character));
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  // Configure VAO/VBO for texture quads
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}
void GUI::text(std::string text, GLfloat x, GLfloat y, GLfloat scale, glm::vec3 color){
  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // Activate corresponding render state
  glUseProgram(fontID);
  glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(640), 0.0f, static_cast<GLfloat>(480));
  send(fontID, projection, "projection");
  glUniform3f(glGetUniformLocation(fontID, "textColor"), color.x, color.y, color.z);
  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  // Iterate through all characters
  std::string::const_iterator c;
  for (c = text.begin(); c != text.end(); c++)
    {
      Character ch = Characters[*c];

      GLfloat xpos = x + ch.Bearing.x * scale;
      GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

      GLfloat w = ch.Size.x * scale;
      GLfloat h = ch.Size.y * scale;
      // Update VBO for each character
      GLfloat vertices[6][4] = {
	{ xpos,     ypos + h,   0.0, 0.0 },
	{ xpos,     ypos,       0.0, 1.0 },
	{ xpos + w, ypos,       1.0, 1.0 },

	{ xpos,     ypos + h,   0.0, 0.0 },
	{ xpos + w, ypos,       1.0, 1.0 },
	{ xpos + w, ypos + h,   1.0, 0.0 }
      };
      // Render glyph texture over quad
      glBindTexture(GL_TEXTURE_2D, ch.TextureID);
      // Update content of VBO memory
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // Be sure to use glBufferSubData and not glBufferData

      // Render quad
      glDrawArrays(GL_TRIANGLES, 0, 6);
      // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
      x += (ch.Advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glUseProgram(0);
  glDisable(GL_BLEND);
}

void Object::read(char * mesh_path){
    //Initialisation
    int nPts, nTri, nNor, nTet, nNorAtV;
    int ver, dim;
    double* tmp = new double[3];
    int refe;

    //READING .mesh
    int inm = GmfOpenMesh(mesh_path,GmfRead,&ver,&dim);
    if ( !inm ){
      std::cout << "Unable to open mesh file " << mesh_path << std::endl;
      exit(-1);
    }

    //GETTING SIZES
    nPts    = GmfStatKwd(inm, GmfVertices);
    nTri    = GmfStatKwd(inm, GmfTriangles);
    if ( !nPts || !nTri ){
      std::cout << "Missing data in mesh file" << mesh_path << std::endl;
      exit(-1);
    }
    vertices.resize(nPts);
    triangles.resize(3 * nTri);

    //VERTICES & INDICES
    GmfGotoKwd(inm,GmfVertices);
    for (int k = 0; k < nPts; k++){
      GmfGetLin(inm,GmfVertices,&tmp[0],&tmp[1],&tmp[2], &refe);
      vertices[k].x = tmp[0];
      vertices[k].y = tmp[1];
      vertices[k].z = tmp[2];
    }
    GmfGotoKwd(inm,GmfTriangles);
    for (int k = 0; k < nTri; k++){
      GmfGetLin(inm,GmfTriangles,&triangles[3*k],&triangles[3*k+1], &triangles[3*k+2], &refe);
      triangles[3*k]-=1;
      triangles[3*k+1]-=1;
      triangles[3*k+2]-=1;
    }

    glm::vec3 mi, ma;
    for(glm::vec3 v : vertices){
      for(int i = 0 ; i < 3 ; i++){
        mi[i] = std::min(v[i], mi[i]);
        ma[i] = std::max(v[i], ma[i]);
      }
    }
    glm::vec3 tr = -0.5f*(ma+mi);
    for(int i = 0 ; i < vertices.size() ; i++){
      vertices[i] += tr;
    }

    std::cout << "Succesfully opened  " << mesh_path << std::endl;
    GmfCloseMesh(inm);
  }
void Object::createNeighbours(){

    int nbh = 0; // Pour parcourir les lignes de neighbours;

    //Pour le triangle triangles[i] trouve les voisins et mets les dans neighbours[nbh]
    for (int i=0; i<triangles.size(); i+=3){

        std::vector<int> tmp;

        //Pour chaque triangle triangles[j] verifie si c'est un voisin du triangle triangles[i]
        //et si c'est le cas mets le dans neighbours[nbh][posvos];
        for(int j=0; j<triangles.size(); j+=3){

            int posvos=0;


            if (triangles[j]==triangles[i] || triangles[j] == triangles[i+1] || triangles[j] == triangles[i+2]
                || triangles[j+1] == triangles[i] || triangles[j+1] == triangles[i+1] || triangles[j+1] == triangles[i+2]
                || triangles[j+2] == triangles[i] || triangles[j+2] == triangles[i+1] || triangles[j+2] == triangles[i+2])

            //Réécriture de la condition:
            //if( (triangles[j]==triangles[i] || triangles[j] == triangles[i+1] || triangles[j] == triangles[i+2]) && (j!=i))
            {
                //Si vrai, le triangle voisin trouvé est triangles[j];
                tmp.push_back(j);
            }
        }
        neighbours.push_back(tmp);
    }
}
std::vector<int> Object::getNeighbours(int ind, int level){
    // Level = 0  - Uniquement l'indice sélectionné
    // Level = 1  - Les premiers voisins de ind

    //std::cout << ind << std::endl;

    //En supposant que ind c'est l'indice obtenu de interesect
    std::vector<std::vector<int>> voisins; // Taillle level * nombre de voisins du level
    voisins.push_back(std::vector<int>{ind});

    for(int i = 1 ; i < level ; i++){//Pour chaque niveau
        std::vector<int> levelNeighbours;//On créé les voisins du niveau
        for (int n : voisins[i-1]){//Pour chaque triangle du niveau précédent
            for(int t : neighbours[n/3]){//Pour chaque voisin du triangle du niveau précédent
                if (std::find(voisins[i-1].begin(), voisins[i-1].end(), t) == voisins[i-1].end()  //Pas dans le niveau - 1
                    && std::find(levelNeighbours.begin(), levelNeighbours.end(), t) == levelNeighbours.end() //Pas dans le niveau
                    //&& std::find(selected.begin(), selected.end(), t) == selected.end())//Pas sélectionné
                    )
                        levelNeighbours.push_back( t );
            }
        }
        voisins.push_back(levelNeighbours);
    }
    std::vector<int> tmp;
    for(std::vector<int> n : voisins)
        tmp.insert(end(tmp), begin(n), end(n));
    std::set<int> neigh_set(tmp.begin(), tmp.end());

    std::vector<int> result = std::vector<int>(neigh_set.begin(), neigh_set.end());

    selected.insert(end(selected), begin(result), end(result));
    std::set<int> selectedSet(selected.begin(), selected.end());
    selected = std::vector<int>(selectedSet.begin(), selectedSet.end());

    return result;

}







