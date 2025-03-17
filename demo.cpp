#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <glad/glad.h>
#include <glfw/include/GLFW/glfw3.h>

/* macros */

#define IX(i,j) ((i)+(N+2)*(j))
#define VIX(i,j) (IX(i,j)*5*2)
#define DIX(i,j) (IX(i,j)*5*6)

/* external definitions (from solver.c) */

extern "C" void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff, float dt );
extern "C" void vel_step ( int N, float * u, float * v, float * u0, float * v0, float visc, float dt );

/* global variables */

static int N;
static float dt, diff, visc;
static float force, source;
static int dvel;

static float * u, * v, * u_prev, * v_prev;
static float * dens, * dens_prev;
static float * vel_vtx_data, * dens_vtx_data;

static GLFWwindow* window;
static int win_x, win_y;
static int mouse_down[3];
static int omx, omy, mx, my;

static GLuint program_id;
static GLuint vel_vao, vel_vbo;
static GLuint dens_vao, dens_vbo;


/*
  ----------------------------------------------------------------------
   free/clear/allocate simulation data
  ----------------------------------------------------------------------
*/


static void free_data ( void )
{
	if ( u ) delete[] u;
	if ( v ) delete[] v;
	if ( u_prev ) delete[] u_prev;
	if ( v_prev ) delete[] v_prev;
	if ( dens ) delete[] dens;
	if ( dens_prev ) delete[] dens_prev;

	if ( vel_vtx_data ) delete[] vel_vtx_data;
	if ( dens_vtx_data ) delete[] dens_vtx_data;
}

static void clear_data ( void )
{
	int i, size=(N+2)*(N+2);

	for ( i=0 ; i<size ; i++ ) {
		u[i] = v[i] = u_prev[i] = v_prev[i] = dens[i] = dens_prev[i] = 0.0f;
	}
}

static int allocate_data ( void )
{
	int size = (N+2)*(N+2);

	u			= new float[size];
	v			= new float[size];
	u_prev		= new float[size];
	v_prev		= new float[size];
	dens		= new float[size];
	dens_prev	= new float[size];

    vel_vtx_data  = new float[5*2*size];
    dens_vtx_data = new float[5*6*size];

	if ( !u || !v || !u_prev || !v_prev || !dens || !dens_prev || !vel_vtx_data || !dens_vtx_data ) {
		fprintf ( stderr, "cannot allocate data\n" );
		return ( 0 );
	}

	return ( 1 );
}


/*
  ----------------------------------------------------------------------
   OpenGL specific drawing routines
  ----------------------------------------------------------------------
*/

static void build_shaders ( void )
{
    // ==== File to cstring ====
    std::ifstream       vs_ifstream,        fs_ifstream;
    std::stringstream   vs_stringstream,    fs_stringstream;
    std::string         vs_string,          fs_string;
    const GLchar        *vs_cstr,           *fs_cstr;
    
    vs_ifstream.open("shaders/vshader.vs");
    fs_ifstream.open("shaders/fshader.fs");
    vs_stringstream << vs_ifstream.rdbuf();
    fs_stringstream << fs_ifstream.rdbuf();
    vs_string = vs_stringstream.str();
    fs_string = fs_stringstream.str();
    vs_cstr = vs_string.c_str();
    fs_cstr = fs_string.c_str();
    
    // ==== Compile shaders ====
    GLuint  vshader,          fshader;
    GLint   vshader_compiled, fshader_compiled;

    vshader = glCreateShader(GL_VERTEX_SHADER);
    fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vshader, 1, &vs_cstr, NULL);
    glShaderSource(fshader, 1, &fs_cstr, NULL);

    glCreateShader(GL_VERTEX_SHADER);
    glCreateShader(GL_FRAGMENT_SHADER);

    glCompileShader(vshader);
    glGetShaderiv(vshader, GL_COMPILE_STATUS, &vshader_compiled);
    if (!vshader_compiled) {
        GLint log_length;
        glGetShaderiv(vshader, GL_INFO_LOG_LENGTH, &log_length);
        char *log = new char[log_length];
        glGetShaderInfoLog(vshader, log_length, &log_length, log);
        printf("Vertex Shader compilation failed: %s\n", log);
        delete[] log;
    }

    glCompileShader(fshader);
    glGetShaderiv(fshader, GL_COMPILE_STATUS, &fshader_compiled);
    if (!fshader_compiled) {
        GLint log_length;
        glGetShaderiv(fshader, GL_INFO_LOG_LENGTH, &log_length);
        char *log = new char[log_length];
        glGetShaderInfoLog(fshader, log_length, &log_length, log);
        printf("Fragment Shader compilation failed: %s\n", log);
        delete[] log;
    }
    
    program_id = glCreateProgram();
    glAttachShader(program_id, vshader);
    glAttachShader(program_id, fshader);

    glLinkProgram(program_id);
    GLint program_linked;
    glGetProgramiv(program_id, GL_LINK_STATUS, &program_linked);
    if (!program_linked)
    {
        GLsizei log_length = 0;
        GLchar message[1024];
        glGetProgramInfoLog(program_id, 1024, &log_length, message);
    }

    glDeleteShader(vshader);
    glDeleteShader(fshader);

    glUseProgram(program_id);
}

static void gen_vertex_arrays ( void )
{
    // Velocity
    glGenVertexArrays(1, &vel_vao);
    glGenBuffers(1, &vel_vbo);

    glBindVertexArray(vel_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vel_vbo);

    glBufferData(GL_ARRAY_BUFFER, 5*2*(N+2)*(N+2)*sizeof(float), NULL, GL_DYNAMIC_DRAW);    // Per simulation coordinate: 2 vertices, each with 5 floats of data
    
    glEnableVertexAttribArray(0);   // Coordinate
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void *)0);
    glEnableVertexAttribArray(2);   // Velocity
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void *)(3*sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    // Density
    glGenVertexArrays(1, &dens_vao);
    glGenBuffers(1, &dens_vbo);

    glBindVertexArray(dens_vao);
    glBindBuffer(GL_ARRAY_BUFFER, dens_vbo);

    glBufferData(GL_ARRAY_BUFFER, 5*6*(N+2)*(N+2)*sizeof(float), NULL, GL_DYNAMIC_DRAW);   // Per simulation coordinate: 4 vertices, each with 5 floats of data
    
    glEnableVertexAttribArray(0);   // Coordinates
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);   // Density
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void *)(2*sizeof(float)));
    glEnableVertexAttribArray(2);   // Velocity
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void *)(3*sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
}

static void pre_display ( void )
{
	glViewport ( 0, 0, win_x, win_y );
	glClearColor ( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear ( GL_COLOR_BUFFER_BIT );
}

static void post_display ( void )
{
	glfwSwapBuffers(window);
}

static void draw_velocity ( void )
{
	int i, j;
	float x, y, h;

	h = 1.0f/N;

	glLineWidth ( 1.0f );

    for ( i=1 ; i<=N ; i++ ) {
        x = (i-0.5f)*h;
        for ( j=1 ; j<=N ; j++ ) {
            y = (j-0.5f)*h;
            
            int l = VIX(i,j);
            vel_vtx_data[VIX(i,j)+0] = x;
            vel_vtx_data[VIX(i,j)+1] = y;
            // vel_vtx_data[VIX(i,j)+2] = 1;
            vel_vtx_data[VIX(i,j)+3] = u[IX(i,j)];
            vel_vtx_data[VIX(i,j)+4] = v[IX(i,j)];

            vel_vtx_data[VIX(i,j)+5+0] = x+u[IX(i,j)];
            vel_vtx_data[VIX(i,j)+5+1] = y+v[IX(i,j)];
            // vel_vtx_data[VIX(i,j)+5+2] = 1;
            vel_vtx_data[VIX(i,j)+5+3] = u[IX(i,j)];
            vel_vtx_data[VIX(i,j)+5+4] = v[IX(i,j)];
        }
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, vel_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 5*2*(N+2)*(N+2)*sizeof(float), vel_vtx_data);

    glUniform1i(glGetUniformLocation(program_id, "drawingDensity"), false);

    glBindVertexArray(vel_vao);
    glDrawArrays(GL_LINES, 0, 2*(N+2)*(N+2));

}

static void draw_density ( void )
{
	int i, j;
	float x, y, h, d00, d01, d10, d11;
    float u00, u01, u10, u11;
    float v00, v01, v10, v11;

	h = 1.0f/N;

    for ( i=0 ; i<=N ; i++ ) {
        x = (i-0.5f)*h;
        for ( j=0 ; j<=N ; j++ ) {
            y = (j-0.5f)*h;

            // Densities and velocities at the cell corners
            d00 = dens[IX(i,  j  )];
            u00 =    u[IX(i,  j  )];
            v00 =    v[IX(i,  j  )];

            d01 = dens[IX(i,  j+1)];
            u01 =    u[IX(i,  j+1)];
            v01 =    v[IX(i,  j+1)];
            
            d10 = dens[IX(i+1,  j)];
            u10 =    u[IX(i+1,  j)];
            v10 =    v[IX(i+1,  j)];

            d11 = dens[IX(i+1,j+1)];
            u11 =    u[IX(i+1,j+1)];
            v11 =    v[IX(i+1,j+1)];


            // Triangle 1
            dens_vtx_data[DIX(i,j)+0] = x;
            dens_vtx_data[DIX(i,j)+1] = y;
            dens_vtx_data[DIX(i,j)+2] = d00;
            dens_vtx_data[DIX(i,j)+3] = u00;
            dens_vtx_data[DIX(i,j)+4] = v00;
            
            dens_vtx_data[DIX(i,j)+5+0] = x+h;
            dens_vtx_data[DIX(i,j)+5+1] = y;
            dens_vtx_data[DIX(i,j)+5+2] = d10;
            dens_vtx_data[DIX(i,j)+5+3] = u10;
            dens_vtx_data[DIX(i,j)+5+4] = v10;
            
            dens_vtx_data[DIX(i,j)+10+0] = x+h;
            dens_vtx_data[DIX(i,j)+10+1] = y+h;
            dens_vtx_data[DIX(i,j)+10+2] = d11;
            dens_vtx_data[DIX(i,j)+10+3] = u11;
            dens_vtx_data[DIX(i,j)+10+4] = v11;
            
            // Triangle 2
            dens_vtx_data[DIX(i,j)+15+0] = x;
            dens_vtx_data[DIX(i,j)+15+1] = y;
            dens_vtx_data[DIX(i,j)+15+2] = d00;
            dens_vtx_data[DIX(i,j)+15+3] = u00;
            dens_vtx_data[DIX(i,j)+15+4] = v00;
            
            dens_vtx_data[DIX(i,j)+20+0] = x+h;
            dens_vtx_data[DIX(i,j)+20+1] = y+h;
            dens_vtx_data[DIX(i,j)+20+2] = d11;
            dens_vtx_data[DIX(i,j)+20+3] = u11;
            dens_vtx_data[DIX(i,j)+20+4] = v11;

            dens_vtx_data[DIX(i,j)+25+0] = x;
            dens_vtx_data[DIX(i,j)+25+1] = y+h;
            dens_vtx_data[DIX(i,j)+25+2] = d01;
            dens_vtx_data[DIX(i,j)+25+3] = u01;
            dens_vtx_data[DIX(i,j)+25+4] = v01;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, dens_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 5*6*(N+2)*(N+2)*sizeof(float), dens_vtx_data);

    glUniform1i(glGetUniformLocation(program_id, "drawingDensity"), true);

    glBindVertexArray(dens_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6*(N+2)*(N+2));
}

/*
  ----------------------------------------------------------------------
   relates mouse movements to forces sources
  ----------------------------------------------------------------------
*/

static void get_from_UI ( float * d, float * u, float * v )
{
	int i, j, size = (N+2)*(N+2);

	for ( i=0 ; i<size ; i++ ) {
		u[i] = v[i] = d[i] = 0.0f;
	}

	if ( !mouse_down[0] && !mouse_down[2] ) return;

	i = (int)((       mx /(float)win_x)*N+1);
	j = (int)(((win_y-my)/(float)win_y)*N+1);

	if ( i<1 || i>N || j<1 || j>N ) return;

	if ( mouse_down[0] ) {
		u[IX(i,j)] = force * (mx-omx);
		v[IX(i,j)] = force * (omy-my);
	}

	if ( mouse_down[2] ) {
		d[IX(i,j)] = source;
	}

	omx = mx;
	omy = my;

	return;
}

/*
  ----------------------------------------------------------------------
   GLFW callback routines
  ----------------------------------------------------------------------
*/

static void key_func (GLFWwindow* WINDOW, int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS)
        return;

    switch ( key )
	{
		case GLFW_KEY_C:
			clear_data ();
			break;

		case GLFW_KEY_Q:
			free_data ();
            glDeleteBuffers(1, &vel_vbo);
            glDeleteBuffers(1, &dens_vbo);
            glDeleteVertexArrays(1, &vel_vao);
            glDeleteVertexArrays(1, &dens_vao);
            glDeleteProgram(program_id);
			glfwTerminate();
            exit ( 0 );
			break;

		case GLFW_KEY_V:
			dvel = !dvel;
			break;
	}
}

static void motion_func (GLFWwindow* window, double xpos, double ypos)
{
	mx = xpos;
	my = ypos;
}

static void reshape_func (GLFWwindow* window, int width, int height)
{
	glfwSetWindowSize(window, width, height);

	win_x = width;
	win_y = height;
}

/*
  ----------------------------------------------------------------------
   Render loop routines
  ----------------------------------------------------------------------
*/

static void process_mouse ( void )
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);;
    omx = mx = x;
	omx = my = y;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        mouse_down[0] = true;
    else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        mouse_down[2] = true;
    else {
        mouse_down[0] = false;
        mouse_down[2] = false;
    }
}

static void compute( void )
{
	
    get_from_UI ( dens_prev, u_prev, v_prev );
	vel_step ( N, u, v, u_prev, v_prev, visc, dt );
	dens_step ( N, dens, dens_prev, u, v, diff, dt );
}

static void display ( void )
{
	pre_display ();

		if ( dvel ) draw_velocity ();
		else		draw_density ();
        
	post_display ();
}


/*
  ----------------------------------------------------------------------
   open_glut_window --- open a glut compatible window and set callbacks
  ----------------------------------------------------------------------
*/

static void open_glfw_window ( void )
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
	window = glfwCreateWindow(win_x, win_y, "Stable Fluids", NULL, NULL);
    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_func);
    glfwSetCursorPosCallback(window, motion_func);
	glfwSetWindowSizeCallback(window, reshape_func);
}


/*
  ----------------------------------------------------------------------
   main --- main routine
  ----------------------------------------------------------------------
*/

int main ( int argc, char ** argv )
{
	glfwInit();

	if ( argc != 1 && argc != 6 ) {
		fprintf ( stderr, "usage : %s N dt diff visc force source\n", argv[0] );
		fprintf ( stderr, "where:\n" );\
		fprintf ( stderr, "\t N      : grid resolution\n" );
		fprintf ( stderr, "\t dt     : time step\n" );
		fprintf ( stderr, "\t diff   : diffusion rate of the density\n" );
		fprintf ( stderr, "\t visc   : viscosity of the fluid\n" );
		fprintf ( stderr, "\t force  : scales the mouse movement that generate a force\n" );
		fprintf ( stderr, "\t source : amount of density that will be deposited\n" );
		exit ( 1 );
	}

	if ( argc == 1 ) {
		N = 128;
		dt = 0.05f;
		diff = 0.00005f;
		visc = 0.002f;
		force = 10.0f;
		source = 250.0f;
		fprintf ( stderr, "Using defaults : N=%d dt=%g diff=%g visc=%g force = %g source=%g\n",
			N, dt, diff, visc, force, source );
	} else {
		N = atoi(argv[1]);
		dt = atof(argv[2]);
		diff = atof(argv[3]);
		visc = atof(argv[4]);
		force = atof(argv[5]);
		source = atof(argv[6]);
	}

	printf ( "\n\nHow to use this demo:\n\n" );
	printf ( "\t Add densities with the right mouse button\n" );
	printf ( "\t Add velocities with the left mouse button and dragging the mouse\n" );
	printf ( "\t Toggle density/velocity display with the 'v' key\n" );
	printf ( "\t Clear the simulation by pressing the 'c' key\n" );
	printf ( "\t Quit by pressing the 'q' key\n" );

	dvel = 0;

	if ( !allocate_data () ) exit ( 1 );
	clear_data ();

	win_x = 1024;
	win_y = 512;
	open_glfw_window ();
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    build_shaders();
    gen_vertex_arrays();

	while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        process_mouse();
        compute();
        display();
    }

	exit ( 0 );
}