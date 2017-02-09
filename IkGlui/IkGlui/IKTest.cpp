#include "stdafx.h"
#include <math.h>
int	main_window;
// the camera info  
float eye[3];
float lookat[3];

// pointers for all of the glui controls
GLUI *glui;
GLUI_Rollout		*object_rollout;
GLUI_RadioGroup		*object_type_radio;
GLUI_Rotation		*object_rotation;
GLUI_Translation	*object_xz_trans;
GLUI_Translation	*object_y_trans;
GLUI_Rollout		*anim_rollout;
GLUI_Button			*action_button;
GLUI_Checkbox *draw_floor_check;
GLUI_Checkbox *draw_object_check;
// This  checkbox utilizes the callback
GLUI_Checkbox *use_depth_buffer;

// the user id's that we can use to identify which control
// caused the callback to be called
#define CB_DEPTH_BUFFER 0
#define CB_ACTION_BUTTON 1
#define CB_RESET 2
#define PI 3.14159265
// walking action variables
GLfloat step = 0;
GLfloat live_anim_speed = 3;

// live variables
// each of these are associated with a control in the interface.
// when the control is modified, these variables are automatically updated
int live_object_type;	// 0=cube, 1=sphere, 2=torus
float live_object_rotation[16];
float live_object_xz_trans[2];
float live_object_y_trans;
int live_draw_floor;
int live_draw_object;
int live_use_depth_buffer;
#define METERS 7

// coordinate for the clicking point
float gX = 0.5, gY = 2.7;
double px[6],py[6];
int winWidth,winHeight;     // window (x,y) size
bool showGrid = false;
float pixPerMeter = 100.0;
double seye[3] = {0.5,0.5,10};
double at[3]  = {0.5,0.5,0};
double up[3]  = {0.0,1.0,0.0};

// Set all variables to their default values.
void reset() {
  showGrid    = false;
  pixPerMeter = 100.0;
  winWidth = winHeight = (int)(pixPerMeter*METERS);
}

void setCamera() {
  glViewport(0,0, winWidth,winHeight);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-winWidth /2/pixPerMeter*.9, winWidth /2/pixPerMeter*.9,
	        -winHeight/2/pixPerMeter*.9, winHeight/2/pixPerMeter*.9, 9, 11);
}

// Draw the coordinate grid.
void drawCoordGrid() {
	double s = -3, t = 4;
	glColor3f(0, 0, 0);
	for (int i = s; i <= t; ++i) {
		// horizontal
		glBegin(GL_LINES);
		glVertex2d(s, double(i));
		glVertex2d(t, double(i));
		glEnd();
		// vertical
		glBegin(GL_LINES);
		glVertex2d(double(i), s);
		glVertex2d(double(i), t);
		glEnd();
	}
}
// draw a line
void drawLine(double x1, double y1, double x2, double y2) {
    glBegin(GL_LINES);
	glVertex2d(x1, y1);
	glVertex2d(x2, y2);
	glEnd();
}

// scalas for each part of the arm
// len: length, wid: width
// ro: rotate angle
// end: end point positions, end[0] represents the fixed end.
double len[5] = { 0.7, 1.35, 1.1, 0.35, 0.4 };
double wid[5] = {1, 0.7, 0.5, 0.35, 0.25};
double ro[5] = { 0, 0, 0, 0, 0};
double end[6][2];
// constrain for this right arm.
double minRo[5] = { -15, -100, -170, -90, -180 };
double maxRo[5] = { 15, 80, 0, 85, 30 };
// fixed end pos of the first part of the arm
double startX = 0.5, startY = -1.2;
// macro for max times of iteration
#define MAX_ITER 10000
// The number of times of IK iteration since the last time we changed the target point
int IkCnt = 0;
// save optimum for cases that can't be reach within MAX_ITER iterations
double bestDis = 1e10;
double bestRo[5] = { 0.0 };


double sumLen(int s, int t) {
	double res = 0;
	for (int i = s; i < t; ++i) {
		res += len[i];
	}
	return res;
}
double sumRo(int s, int t) {
	double res = 0;
	for (int i = s; i < t; ++i) {
		res += ro[i];
	}
	return res;
}
double min(double &a, double &b) {
	return a < b ? a : b;
}
double max(double &a, double &b) {
	return a < b ? b : a;
}
// Calcuate end1 end2 arrays according to len and ro arrays
void updateEndPos() {
	end[0][0] = startX;
	end[0][1] = startY;
	for (int i = 1; i <= 5; ++i){
		double totRo = sumRo(0, i);
		end[i][0] = end[i - 1][0] + len[i - 1] * sin(totRo * PI / 180);
		end[i][1] = end[i - 1][1] + len[i - 1] * cos(totRo * PI / 180);
	}
}


void drawRect(double x1, double y1, double x2, double y2) {
	glColor3f(0, 0, 1);
	glBegin(GL_POLYGON);
	glVertex2f(x1, y1);
	glVertex2f(x2, y1);
	glVertex2f(x2, y2);
	glVertex2f(x1, y2);
	glEnd();
}

void drawSide(double w, double l) {
	glColor3f(0, 1, 0);
	glLineWidth(5.0);
	glBegin(GL_LINES);
	glVertex2f(w / -2.0, 0);
	glVertex2f(w / -2.0, 0.7 * l);
	glEnd();
	glBegin(GL_LINES);
	glVertex2f(w / 2.0, 0);
	glVertex2f(w / 2.0, 0.7 * l);
	glEnd();
	glBegin(GL_LINES);
	glVertex2f(w / -2.0, 0.7 * l);
	glVertex2f(0, l);
	glEnd();
	glBegin(GL_LINES);
	glVertex2f(w / 2.0, 0.7 * l);
	glVertex2f(0, l);
	glEnd();
	glBegin(GL_LINES);
	glVertex2f(w / -2.0, 0);
	glVertex2f(w / 2.0, 0);
	glEnd();
}

void drawTri(double w, double l) {
	glBegin(GL_POLYGON);
	glVertex2f(w / -2.0, 0.7 * l);
	glVertex2f(0, l);
	glVertex2f(w / 2.0, 0.7 * l);
	glEnd();
}

void drawArm() {
	glTranslatef(startX, startY, 0);
	for (int i = 0; i < 5; ++i) {
		if (i % 2 == 0)
			glColor3f(0, 0, 1);
		else
			glColor3f(0, 1, 0);
		glRotatef(ro[i], 0, 0, -1);
		//drawLine(0, 0, 0, len[i]);
		drawRect(wid[i] / -2.0, 0, wid[i] / 2.0, 0.7 * len[i]);
		drawTri(wid[i], len[i]);
		drawSide(wid[i], len[i]);
		if (i < 4)
			glTranslatef(0, len[i], 0);
	}
}


void drawTarget(double x, double y, double a){
	glColor3f(1, 1, 0);
	double cos72 = cos(72.0 / 180 * PI), sin72 = sin(72.0 / 180 * PI);
	double cos36 = cos(36.0 / 180 * PI), sin36 = sin(36.0 / 180 * PI);
	double l1 = a * sin72;
	double l2 = a * cos72;
	double smallRadius = l2 / sin36;
	double l3 = smallRadius * cos36;
	double radius =  l1 + l3;
	double vertex[10][3] = {0.0};
	vertex[0][0] = x;						vertex[0][1] = y + radius;
	vertex[1][0] = x + l2;					vertex[1][1] = y + l3;
	vertex[2][0] = x + l2 + a;				vertex[2][1] = y + l3;
	vertex[3][0] = x + smallRadius * sin72; vertex[3][1] = y - smallRadius * cos72;
	vertex[4][0] = x + radius * sin36;		vertex[4][1] = y - radius * cos36;
	vertex[5][0] = x;						vertex[5][1] = y - smallRadius;
	vertex[6][0] = x - radius * sin36;		vertex[6][1] = y - radius * cos36;
	vertex[7][0] = x - smallRadius * sin72; vertex[7][1] = y - smallRadius * cos72;
	vertex[8][0] = x - l2 - a;				vertex[8][1] = y + l3;
	vertex[9][0] = x - l2;					vertex[9][1] = y + l3;
	for (int i = 0; i < 10; ++i) {
		glBegin(GL_POLYGON);
		glVertex2f(x, y);
		glVertex2f(vertex[i][0], vertex[i][1]);
		glVertex2f(vertex[(i + 1) % 10][0], vertex[(i + 1) % 10][1]);
		glEnd();
	}
	
}

double calcDis(double x1, double y1, double x2, double y2) {
	return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

// Functions for IK algorithm
void calcJ(double J[2][5]) {
	for (int i = 0; i < 5; ++i) {
		double a = end[5][0] - end[i][0];
		double b = end[5][1] - end[i][1];
		J[0][i] = b;
		J[1][i] = -a;
	}
}

void calcInv(double JtJ[5][5], double invJtJ[5][5]) {
	double matrix[5][10] = { 0.0 };
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 5; ++j)
			matrix[i][j] = JtJ[i][j];
	int i, j, k, n = 5;
	for (i = 0; i < n; i++){
		for (j = n; j < 2 * n; j++){
			if (i == (j - n))
				matrix[i][j] = 1.0;
			else
				matrix[i][j] = 0.0;
		}
	}
	for (i = 0; i < n; i++){
		for (j = 0; j < n; j++){
			if (i != j){
				float ratio = matrix[j][i] / matrix[i][i];
				matrix[j][i] = 0.0;
				for (k = i + 1; k < 2 * n; k++){
					matrix[j][k] -= ratio * matrix[i][k];
				}
			}
		}
	}
	for (i = 0; i < n; i++){
		double a = matrix[i][i];
		for (j = 0; j < 2 * n; j++){
			matrix[i][j] /= a;
		}
	}
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 5; ++j)
			invJtJ[i][j] = matrix[i][5 + j];
}

void calcJplus(double J[2][5], double Jp[5][2]) {
	double Jt[5][2];
	// calc transpose of J
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 2; ++j)
			Jt[i][j] = J[j][i];
	// calc Jt * J
	double JtJ[5][5];
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 5; ++j) {
			JtJ[i][j] = 0.000000000000;
			for (int k = 0; k < 2; ++k)
				JtJ[i][j] += Jt[i][k] * J[k][j];
			char buf[20];
			sprintf(buf, "%.6f", JtJ[i][j]);
			sscanf(buf, "%f", &JtJ[i][j]);
		}
	// calc invJtJ
	double invJtJ[5][5] = { 0.0 };
	calcInv(JtJ, invJtJ);
	// calc J+
	for (int i = 0; i < 5; ++i)
		for (int j = 0; j < 2; ++j) {
			Jp[i][j] = 0.0;
			for (int k = 0; k < 5; ++k) {
				Jp[i][j] += invJtJ[i][k] * Jt[k][j];
			}
		}
}

void updateArm(double tx, double ty) {
	int cnt = 0;
	double J[2][5];
	double Jp[5][2];

	calcJ(J);
	calcJplus(J, Jp);
	double deltaE[2] = { (tx - end[5][0]) / 100, (ty - end[5][1]) / 100 };
	double deltaRo[5];
	for (int i = 0; i < 5; ++i){
		deltaRo[i] = 0.0;
		for (int k = 0; k < 2; ++k)
			deltaRo[i] += Jp[i][k] * deltaE[k];
	}
	for (int i = 0; i < 5; ++i) {
		ro[i] += deltaRo[i] / PI * 180;
		ro[i] = min(maxRo[i], ro[i]);
		ro[i] = max(minRo[i], ro[i]);
	}
	updateEndPos();
}

void myGlutIdle(void)
{
	// make sure the main window is active
	if (glutGetWindow() != main_window)
		glutSetWindow(main_window);
	if (calcDis(end[5][0], end[5][1], gX, gY) <= 0.05)
		IkCnt = 0; 
	else if (IkCnt++ < MAX_ITER) {
		updateArm(gX, gY);
		// check to save optimum position
		double curDis = calcDis(end[5][0], end[5][1], gX, gY);
		if (curDis < bestDis) {
			bestDis = curDis;
			for (int i = 0; i < 5; ++i)
				bestRo[i] = ro[i];
		}
	}
	else if (IkCnt++ == MAX_ITER + 1) {
		for (int i = 0; i < 5; ++i) {
			ro[i] = bestRo[i];
		}
		updateEndPos();
	}

	// just keep redrawing the scene over and over
	glutPostRedisplay();
}


// mouse handling functions for the main window
// left mouse translates, middle zooms, right rotates
// keep track of which button is down and where the last position was
int cur_button = -1;
// catch mouse up/down events
void myGlutMouse(int button, int state, int x, int y)
{
	if (state == GLUT_DOWN)
		cur_button = button;
	else if (button == cur_button) {
		cur_button = -1;
	}
	gX = ((float)(x))/winWidth * METERS - 3;
	gY = (-((float)(y))/winHeight * METERS)+ 4;
	IkCnt = 0;
	bestDis = 1e10;
	// Leave the following call in place.  It tells GLUT that
	// we've done something, and that the window needs to be
	// re-drawn.  GLUT will call display().
	glutPostRedisplay();
}

// catch mouse move events
void myGlutMotion(int x, int y) {
	glutPostRedisplay();
}

// you can put keyboard shortcuts in here
void myGlutKeyboard(unsigned char key, int x, int y)
{
	switch(key)
	{
	// quit
	case 27: 
	case 'q':
	case 'Q':
		exit(0);
		break;
	}

	glutPostRedisplay();
}

// the window has changed shapes, fix ourselves up
void myGlutReshape(int	x, int y)
{
	int tx, ty, tw, th;
	GLUI_Master.get_viewport_area(&tx, &ty, &tw, &th);
	glViewport(tx, ty, tw, th);

	glutPostRedisplay();
}

// draw the scene
void myGlutDisplay(	void )
{
	setCamera();
	// Set the background colour to dark gray.
	glClearColor(.90f,.90f, .9f,1.f);
    // OK, now clear the screen with the background colour
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_LINE_SMOOTH);
    // Position the camera eye and look-at point.
	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(seye[0],seye[1],seye[2],  at[0],at[1],at[2],  up[0],up[1],up[2]);
	// draw the coordinate grid, if necessary.
	if (showGrid) 
		drawCoordGrid();
	// Draw the target point as a star.
	drawTarget(gX, gY, 0.15);
	// Draw the whole arm.
	drawArm();
	// Execute any GL functions that are in the queue.
	glFlush();
	// Now, show the frame buffer that we just drew into.
	glutSwapBuffers();
}
 
int _tmain(int argc, _TCHAR *argv[])
{
  reset();
  updateEndPos();
  // Initialize the GLUT window.
  glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
  // Put the window at the top left corner of the screen.
  glutInitWindowPosition (0, 0);
  // Specify the window dimensions.
  glutInitWindowSize(winWidth,winHeight);
  // And now create the window.
  main_window = glutCreateWindow("IK Canvas");
  // set callbacks
  glutDisplayFunc(myGlutDisplay);
  GLUI_Master.set_glutReshapeFunc(myGlutReshape);
  GLUI_Master.set_glutIdleFunc(myGlutIdle);
  GLUI_Master.set_glutKeyboardFunc(myGlutKeyboard);
  GLUI_Master.set_glutMouseFunc(myGlutMouse);
  glutMotionFunc(myGlutMotion);
  // initialize gl
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_COLOR_MATERIAL);
  // give control over to glut
  glutMainLoop();
}