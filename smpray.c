/*  
*
* File:         smpray.c
* Description:  This is Ben's demo ported to OpenGL and pthreads.
* Author:       Bob Kozdemba 
* Contributors: Ben Garlick
* Created:      22-Mar-05
* Last modified: Wed Feb 08, 2012  09:41AM CST
*
*/

// Changes from Ben's orginal code:
// - Uses pthreads and semaphores versus Irix sproc() and barrier()
// - Render threads work on adjacent scanlines instead of chunks. 
// - Added specular lighting.
// - Fixed incorrect R2 constant for blue sphere.
// - Added command line options to specify number of threads, 
//   size and zoom, etc.

#define N_THREADS 128		/* max number of threads */
#define R_OK 4

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
// #include <sys/signal.h>
// #include <sys/prctl.h>
#include <sys/stat.h>
// #include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#ifdef WIN32
#include <io.h>
#else
// #include <unistd.h>
#endif

#include <math.h>
#include <GL/glut.h>

#define SPHERES 3
// Light source direction
#define LX (0.707)
#define LY (0.707)
#define LZ (0.707)
#define REFLECT .5
#define DIFF .5
#define focal .7
#define ZEYE 1.
#define GRID 11
#define YPLANE (-.2)
#define PLANE


extern int getopt (int argc, char **argv, char *opts);

int ray (int level, int sph, float x, float y, float z,
	 float rx, float ry, float rz, float *r, float *g, float *b);

int init_plane (void);
int init_spheres (void);

float rotx = 0., roty = 0.;
int oldx = 0, oldy = 0, do_xform = 0;
pthread_mutex_t x_lock;

float plane_r[GRID][GRID];
float plane_g[GRID][GRID];
float plane_b[GRID][GRID];

GLfloat M[4][4];
static char str[80];

// Global array to hold the rendered image.
// unsigned char clr[size][size][4];
// unsigned char clrRight[size][size][4];
unsigned char *clr;
/* Right eye used for stereo. */
unsigned char *clrRight;
/* Image size */
long size = 512;
short zoom = 1;
int stereoOn = 0;

float SX[SPHERES];
float SY[SPHERES];
float SZ[SPHERES];
float CX[SPHERES];
float CY[SPHERES];
float CZ[SPHERES];
float R2[SPHERES];
float R[SPHERES];
float DR[SPHERES];
float DG[SPHERES];
float DB[SPHERES];

// Global temp used by the tracer.
float t;

//
// Thread info structure
//
typedef struct ThreadInfo
{
  int num;			// The total number of threads being used.
  int id;			// Local thread ID.
} _thread_info;


// The total number of threads being used.
int NumProcs = 1;

_thread_info info[N_THREADS];
pthread_t thread[N_THREADS];

// Arrays of semaphores used for syncing the render threads with main.
sem_t sem1[N_THREADS];
sem_t render_done[N_THREADS];

int leftButtonState = 0;
int freeRun = 1;
int printStats = 0;
int displayIsOn = 1;

void
idle (void)
{

  glutPostRedisplay ();

}

void display (void);

//
// do_it - Each thread that is created will run this routine.
//
// Params - info: A pointer to the thread info.
//
void *
do_it (void *info)
{
  int x, y;
  float rl, gl, bl;
  _thread_info *_info = (_thread_info *) info;

  while (1)
    {

#ifdef DEBUG
      printf ("thread %d is waiting for go.\n", _info->id);
      fflush (stdout);
#endif

      // Wait for main to say "go"
      sem_wait (&sem1[_info->id]);

#ifdef DEBUG
      printf ("Thread %d running\n", _info->id);
      fflush (stdout);
#endif

      // Ray trace my portion of the scene. The y increment determines which
      // scanlines are traced in this thread.
      for (y = _info->id; y < size; y += _info->num)
	{

	  for (x = 0; x < size; x++)
	    {


	      float mag;
	      float dx, dy;

	      // setup the ray direction.
	      dx = 1.0 * (x - size / 2) / size;
	      dy = 1.0 * (y - size / 2) / size;

	      mag = 1. / sqrt (dx * dx + dy * dy + (focal * focal));

	      // Fire the ray. The pixel color is returned into r, g, b.
	      rl = .0;
	      gl = .0;
	      bl = .0;
	      ray (0, -1, 0., 0., ZEYE, dx * mag, dy * mag, (-focal) * mag,
		   &rl, &gl, &bl);

	      //
	      // Stuff the color into the global image array. Since each x and y
	      // is unique, this is thread safe.
	      //
	      // pthread_mutex_lock(&x_lock);
	      long index = (4 * size * y) + (4 * x);
	      clr[index] = (int) (rl);
	      clr[index + 1] = (int) (gl);
	      clr[index + 2] = (int) (bl);
	      clr[index + 3] = 255;
	      // clr[y][x][0] = (int) (rl);
	      // clr[y][x][1] = (int) (gl);
	      // clr[y][x][2] = (int) (bl);
	      // clr[y][x][3] = 255;
	      // pthread_mutex_unlock(&x_lock);

	      // if(stereoOn)
	      if (stereoOn)
		{
		  float red, green, blue;
		  red = 0.;
		  green = 0.;
		  blue = 0.;
		  // Fire the ray. The pixel color is returned into r, g, b.
		  ray (0, -1, 0., 0., ZEYE, dx * mag, dy * mag,
		       (-focal) * mag, &red, &green, &blue);

		  //
		  // Stuff the color into the global image array. Since each x and y
		  // is unique, this is thread safe.
		  //
		  // clrRight[y][x][0] = (int) (red);
		  // clrRight[y][x][1] = (int) (green);
		  // clrRight[y][x][2] = (int) (blue);
		  // clrRight[y][x][3] = 255;
		  clrRight[index] = (int) (rl);
		  clrRight[index + 1] = (int) (gl);
		  clrRight[index + 2] = (int) (bl);
		  clrRight[index + 3] = 255;

		}

	    }			// for x

	}			// for y

#ifdef DEBUG
      printf ("Thread %d is finished.\n", _info->id);
      fflush (stdout);
#endif

      // Tell main I'm finished.
      sem_post (&render_done[_info->id]);

    }				// while 

}

//
// getClock - Returns elapsed time in seconds.
//
double
getClock (void)
{

  // struct timeval t; 

  // gettimeofday( &t, NULL ); 

  // return (double) t.tv_sec + (double) t.tv_usec * 1E-6; 
  return 1.0;

}

//
// mouse - called by glut when a mouse button events occurs.
//
void
mouse (int button, int state, int x, int y)
{


  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
    {
      leftButtonState = 1;
      oldx = x;
      oldy = y;
    }
  if (button == GLUT_LEFT_BUTTON && state == GLUT_UP)
    {
      leftButtonState = 0;

    }

}

//
// bitmap_output - Draws a string at the given location
//
void
bitmap_output (int x, int y, char *string, void *font)
{

  int len, i;

  glRasterPos2f (x, y);
  len = (int) strlen (string);
  for (i = 0; i < len; i++)
    {
      glutBitmapCharacter (font, string[i]);
    }
}

//
// render - This function positions the spheres, wakes up the render threads,
//          waits for them to finish and draws the resulting image. The
//          winx and winy vars are used to position the spheres. 
//
void
render (int winx, int winy)
{

  int i, j, k;
  float x, y, z;
  // static int oldx = size / 2;
  // static int oldy = size / 2;

  static float mat[4][4] = { 1., 0., 0., 0.,
    0., 1., 0., 0.,
    0., 0., 1., 0.,
    0., 0., 0., 1.,
  };

  // Grab the modelview matrix and use it to move the sphere origins.
  glPushMatrix ();

  glLoadIdentity ();
  glRotatef (roty, 0, 1, 0);
  glMultMatrixf (&mat[0][0]);
  glGetFloatv (GL_MODELVIEW_MATRIX, &mat[0][0]);

  glLoadIdentity ();
  glRotatef (rotx, 1, 0, 0);
  glMultMatrixf (&mat[0][0]);
  glGetFloatv (GL_MODELVIEW_MATRIX, &mat[0][0]);
  rotx = roty = 0.0;
  do_xform = 0;

  glGetFloatv (GL_MODELVIEW_MATRIX, &M[0][0]);
  glPopMatrix ();

  // Move the sphere origins.
  for (i = 0; i < SPHERES; i++)
    {
      x = SX[i];
      y = SY[i];
      z = SZ[i];
      CX[i] = x * M[0][0] + y * M[1][0] + z * M[2][0];
      CY[i] = x * M[0][1] + y * M[1][1] + z * M[2][1];
      CZ[i] = x * M[0][2] + y * M[1][2] + z * M[2][2];
    }

  t += .1;
  SY[0] = sin (t) * .2;
  SY[1] = sin (t * 1.3) * .2;
  SY[2] = sin (t * 1.7) * .2;

#ifdef DEBUG
  // Clear array for debugging
  for (i = 0; i < size; i += 1)
    for (j = 0; j < size; j += 1)
      for (k = 0; k < 4; k += 1)
	clr[i][j][k] = (unsigned char) 0;
  printf ("main: Waking up threads\n");
  fflush (stdout);
#endif


  // Wake up the threads. If a condition variable broadcast were used
  // instead of semaphores, would the threads get awakened in parallel
  // on an SMP scheduler?
  for (i = 0; i < NumProcs; i += 1)
    {
      sem_post (&sem1[i]);
    }


  // Wait for the threads to finish their work.
  for (i = 0; i < NumProcs; i += 1)
    {
#ifdef DEBUG
      printf ("main: Waiting on worker semaphore.\n");
      fflush (stdout);
#endif

      sem_wait (&render_done[i]);

#ifdef DEBUG
      printf ("main: Thread %d is finished\n", i);
      fflush (stdout);
#endif
    }

#ifdef DEBUG
  printf ("Frame done.\n");
  fflush (stdout);
#endif

}

void
drawImage ()
{

  // Draw the image.
  if (displayIsOn)
    {
      if (stereoOn)
	{
	  pthread_mutex_lock (&x_lock);
	  glDrawBuffer (GL_BACK_RIGHT);
	  glDrawPixels (size, size, GL_RGBA, GL_UNSIGNED_BYTE, clr);
	  // glRasterPos2f( -1, -1 );
	  // bitmap_output(-1, -1, str,GLUT_BITMAP_TIMES_ROMAN_24);
	  glDrawBuffer (GL_BACK_LEFT);
	  glDrawPixels (size, size, GL_RGBA, GL_UNSIGNED_BYTE, clr);
	  // glRasterPos2f( -1, -1 );
	  // bitmap_output(-1, -1, str,GLUT_BITMAP_TIMES_ROMAN_24);
	  pthread_mutex_unlock (&x_lock);
	}
      else
	{
	  // glClearColor( 0, 0, 0, 0 );
	  // glClear(GL_COLOR_BUFFER_BIT);
	  glPixelZoom (zoom, zoom);

	  glRasterPos2f (-1, -1);

	  glDrawPixels (size, size, GL_RGBA, GL_UNSIGNED_BYTE, clr);
	  // Display the frame rate of the previous frame.
	  bitmap_output (-1, -1, str, GLUT_BITMAP_TIMES_ROMAN_24);
	}


    }


}

//
// motion - called by glut when a mouse motion events occurs.
//
void
motion (int winx, int winy)
{


#ifdef DEBUG
  printf ("motion event: %d %d \n", winx, winy);
  printf ("leftButtonState: %d\n", leftButtonState);
  fflush (stdout);
#endif

  if (leftButtonState == 1)
    {
//    render( oldx, oldy );

      // update the transformations

      rotx += (float) winy - oldy;
      roty -= (float) oldx - winx;

      oldx = winx;
      oldy = winy;
      do_xform = 1;
    }
  glutPostRedisplay ();
}

//
// display - This function is typically called by an expose, mouse 
//           event.

void
display (void)
{
  static int t0;
  static int t1;

  static int swapBufs = 0;
  // static int x = size / 2;
  // static int y = size / 2;
  static int nFrames = 0;

#ifdef DEBUG
  printf ("display called.\n");
  fflush (stdout);
#endif

  // Start the frame timer.
  if (nFrames == 0)
    {
      t0 = glutGet (GLUT_ELAPSED_TIME);
    }

  // render( x++, y++ );
  render (300, 300);

  drawImage ();

  glutSwapBuffers ();
  nFrames += 1;

  // Stop the frame timer and save the elapsed time.
  // t1 = getClock();
  if (nFrames == 10)
    {
      t1 = glutGet (GLUT_ELAPSED_TIME);
      sprintf (str, "%f Hz", nFrames * 1. / (t1 - t0) * 1000.);
      if (printStats)
	{
	  printf ("%f Hz\n", nFrames * 1. / (t1 - t0) * 1000.);
	}
      nFrames = 0;
    }

}

//
// mouse - called by glut when a keyboard events occurs.
//
void
keyboard (unsigned char key, int x, int y)
{

  switch (key)
    {
    case 27:
      exit (0);
      break;
    }

}


main (int argc, char **argv)
{

  pthread_attr_t attr;
  int i;
  char winname[80];
  int c;

  int errflg = 0;

  extern char *optarg;
  extern int optind, optopt;

  // process the cmd line args.
  while ((c = getopt (argc, argv, "spmnt:g:z:")) != -1)
    {
      switch (c)
	{
	case 'g':
	  size = atoi (optarg);
	  break;
	case 'z':
	  zoom = atoi (optarg);
	  break;
	case 's':
	  stereoOn = 1;
	  printf ("Stereo is enabled.\n");
	  break;
	case 'p':
	  printStats = 1;
	  break;
	case 'n':
	  displayIsOn = 0;
	  break;
	case 'm':
	  freeRun = 0;
	  break;
	case 't':
	  NumProcs = atoi (optarg);
	  if (NumProcs > N_THREADS)
	    {
	      printf ("Maximum number of threads is %d!\n", N_THREADS);
	      printf ("Sounds like you have an SGI!\n");
	      exit (1);
	    }
	  break;
	case ':':		/* -f or -o without operand */
	  fprintf (stderr, "Option -%c requires an operand\n", optopt);
	  errflg++;
	  break;
	case '?':
	  fprintf (stderr, "Unrecognized option: -%c\n", optopt);
	  errflg++;
	}
    }
  if (errflg)
    {
      fprintf (stderr,
	       "usage: %s [-t threads] [-g size] [-z zoom] [-m] [-p] [-n]\n",
	       argv[0]);
      exit (2);
    }
  for (; optind < argc; optind++)
    {
      if (access (argv[optind], R_OK))
	{

	}

    }

  sprintf (winname, "CrazyBalls SMP RayTracer: Threads = %d", NumProcs);

  clr = (unsigned char *) malloc (size * size * 4);
  clrRight = (unsigned char *) malloc (size * size * 4);

  if (clr == NULL)
    {
      fprintf (stderr, "Malloc error!\n");
      exit (0);
    }


  // Setup some pthread attributes.
  pthread_setconcurrency (NumProcs);
  pthread_attr_init (&attr);
  pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_mutex_init (&x_lock, NULL);

  init_plane ();
  init_spheres ();

  // Init semaphores to 0 and create the threads.
  for (i = 0; i < NumProcs; i += 1)
    {
      sem_init (&render_done[i], 0, 0);
      sem_init (&sem1[i], 0, 0);
      info[i].num = NumProcs;
      info[i].id = i;
      pthread_create (&thread[i], NULL, do_it, &info[i]);
    }

  // Setup glut.
  // XInitThreads();
  glutInit (&argc, argv);
  if (stereoOn)
    {
      glutInitDisplayMode (GLUT_STEREO | GLUT_DOUBLE | GLUT_RGB);
    }
  else
    {
      glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGBA);
    }
  glutInitWindowSize (size * zoom, size * zoom);
  glutCreateWindow (winname);
  glutDisplayFunc (display);
  glutKeyboardFunc (keyboard);
  glutMouseFunc (mouse);
  glutMotionFunc (motion);
  if (freeRun)
    {
      glutIdleFunc (idle);
    }
  glutMainLoop ();

  // In theory we never get here.
  for (i = 0; i < NumProcs; i += 1)
    {
      pthread_join (thread[i], NULL);
    }


}

//
// init_spheres
//
init_spheres ()
{

  SX[0] = -.2;
  SY[0] = 0.;
  SZ[0] = 0.;
  R2[0] = .04;
  R[0] = .2;
  DR[0] = .7 * 255.;
  DG[0] = .2 * 255.;
  DB[0] = .1 * 255.;

  SX[1] = .2;
  SY[1] = 0.;
  SZ[1] = 0.;
  R2[1] = .04;
  R[1] = .2;
  DR[1] = 1. * 255.;
  DG[1] = 1. * 255.;
  DB[1] = 1. * 255.;

  SX[2] = .05;
  SY[2] = 0.;
  SZ[2] = -.3;
  R2[2] = .04;
  R[2] = .2;
  DR[2] = .1 * 255.;
  DG[2] = .2 * 255.;
  DB[2] = .8 * 255.;

}

#ifdef WIN32

int
random (void)
{
  return rand ();
}
#endif

//
// init_plane
//
init_plane ()
{
  int x, y;
  unsigned int color = 0;
  const unsigned short brightColor = 255;

  for (y = 0; y < GRID; y++)
    for (x = 0; x < GRID; x++)
      {
//	plane_r[y][x] = 255. * (random () & 0xffff) / 65535.0;
//	plane_g[y][x] = 255. * (random () & 0xffff) / 65535.0;
//	plane_b[y][x] = 255. * (random () & 0xffff) / 65535.0;
// 
// Alternate the color between light and dark. This assumes GRID is odd.
//
  color = (y * GRID + x) % 2 * brightColor;
	plane_r[y][x] = plane_g[y][x] = plane_b[y][x] = (float) color;
      }

}

//
// ray - The main ray tracer is hard coded for 3 spheres and a plane.
//       This is essentially untouched from Ben's code with the exception
//       of the specular highlights.
//
ray (int level, int sph, float x, float y, float z, float rx, float ry,
     float rz, float *r, float *g, float *b)
{


  float disc, v;
  float ix, iy, iz;
  float nx, ny, nz;
  float dot;
  float rr, rg, rb;
  float diff;
  float dmin;
  int num;
  int XX, YY;
  int s;
  float halfway[3] = { 0.3574, 0.3574, 0.8629 };
  float spec;

  dmin = 999999.9;
  num = -1;			// How many spheres were intersected?

  for (s = 0; s < SPHERES; s++)
    {
      if (sph != s)
	{
	  nx = CX[s] - x;	/* nx,ny,nz = vector from eye to sphere cntr */
	  ny = CY[s] - y;
	  nz = CZ[s] - z;

	  v = nx * rx + ny * ry + nz * rz;
	  if (v > 0.)
	    {
	      disc = R2[s] - (nx * nx + ny * ny + nz * nz - v * v);
	      if (disc > 0.)
		{
		  disc = v - sqrt (disc);
		  if (disc < dmin)
		    {
		      dmin = disc;
		      num = s;
		    }
		}
	    }

	}			// if 
    }				// for

#ifdef PLANE

  if (ry < 0. && y > (YPLANE + .001))
    {				/*plane */
      v = -(y - (YPLANE)) / ry;
      if (v < dmin)
	{
	  ix = x + v * rx;
	  iz = z + v * rz;
	  if (ix > -.6 && ix < .6 && iz > -.6 && iz < .6)
	    {
	      iy = YPLANE;
	      ry = -ry;
	      num = -1;
	      XX = (int) (GRID * (ix + .6) / 1.2);
	      YY = (int) (GRID * (iz + .6) / 1.2);

	      rr = rg = rb = 0.;
	      if (level < 3)
		ray (level + 1, num, ix, iy, iz, rx, ry, rz, &rr, &rg, &rb);
	      diff = DIFF * (LY);
	      *r += plane_r[YY][XX] * diff + REFLECT * rr;
	      *g += plane_g[YY][XX] * diff + REFLECT * rg;
	      *b += plane_b[YY][XX] * diff + REFLECT * rb;

	      return;

	    }
	}

    }
#endif

  if (num >= 0)
    {
      ix = x + dmin * rx;
      iy = y + dmin * ry;
      iz = z + dmin * rz;
      v = 1. / R[num];		/* one over radius */
      nx = (ix - CX[num]) * v;
      ny = (iy - CY[num]) * v;
      nz = (iz - CZ[num]) * v;
      dot = 2. * (nx * rx + ny * ry + nz * rz);
      rx = rx - dot * nx;
      ry = ry - dot * ny;
      rz = rz - dot * nz;
      /*
         printf("r %f %f %f   n %f %f %f   b %f %f %f\n",
         rx,ry,rz,nx,ny,nz,bx,by,bz);
       */

      rr = rg = rb = 0.;

      if (level < 3)
	ray (level + 1, num, ix, iy, iz, rx, ry, rz, &rr, &rg, &rb);
      diff = DIFF * (nx * LX + ny * LY + nz * LZ);
      spec = pow (halfway[0] * nx + halfway[1] * ny + halfway[2] * nz, 1000);

      if (diff < 0.)
	{
	  *r += REFLECT * rr;
	  *g += REFLECT * rg;
	  *b += REFLECT * rb;

	}
      else
	{
	  *r += DR[num] * diff + REFLECT * rr;
	  *g += DG[num] * diff + REFLECT * rg;
	  *b += DB[num] * diff + REFLECT * rb;
	  *r += REFLECT * spec * 255;
	  *g += REFLECT * spec * 255;
	  *b += REFLECT * spec * 255;

	  if (*r > 255.)
	    *r = 255;
	  if (*g > 255.)
	    *g = 255;
	  if (*b > 255.)
	    *b = 255;

	}


    }				// if 

}
