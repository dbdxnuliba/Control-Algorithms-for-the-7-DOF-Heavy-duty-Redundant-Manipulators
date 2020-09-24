//****************************************INCLUDING***************************************************
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <fstream>
#include <iostream>
#include "downscale4/one.h" 
#include "downscale4/two.h" 
#include "downscale4/three.h" 
#include "downscale4/seven.h"
#include "downscale4/trans.h"
#include "downscale4/buttons.h"
#include "downscale4/charmsg.h"

#include "ros/ros.h"
#include "ros/time.h"

#include </home/nuclear/eigen/Eigen/Dense>
#include </home/nuclear/eigen/pseudoinverse.cpp>
#include <boost/bind.hpp>
#include <gazebo/gazebo.hh>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include "PowerPMACcontrol.h"
#include "argParser.h"

//************************************GLOBAL DEFINITION**********************************************
#define pi 3.14159265359
#define g_acc 9.81

using namespace std;
using namespace PowerPMACcontrol_ns;
using Eigen::MatrixXd;
//*************************************ARM CONFIGURATION**********************************************
//Arm offset (m)
#define d1 0.371
#define d3 0.547
#define d5 0.484
#define d7 0.273

//Center of mass for each joint, from local coordinate (m)
#define lg1 0.247
#define lg2 0.32328
#define lg3 0.24043
#define lg4 0.52448
#define lg5 0.21711
#define lg6 0.46621
#define lg7 0.15735
#define lge 0.02301

//Joint mass (kg)
double m[7];

std::string sep = "\n----------------------------------------\n";
//***********************************MASTER DEVICE SUBCRIBER******************************************
double Output_data[16];             //4x4 homogeneous trans from master device - hd_callback_trans
double hd_rotation_matrix[9];       //3x3 rotation matrix coverted from Output_data - hd_callback_trans
double hd_current_xyz[3];           //current position
double hd_del_xyz[3];               //position errors
double hd_initial_xyz[3];           //init position
double robot_prev_xyz[3];           //previous position
double robot_del_xyz[3];            //Differential position
double robot_initial_xyz[3];        //Init EE position
double haptic_del_xyz[3];

int Buttons[2];                     //Phantom omni button variables - hd_callback_buttons
int Inkwell=0;                      //Phantom omni Inkwell button - hd_callback_buttons
int button0_click_time = 0;         //Buttons[0] clicked counting variable
int button1_click_time = 0;
double Euler_r, Euler_b, Euler_a;
double time_taken = 0.003;

MatrixXd robot_initial_rotation(3,3); 		//
MatrixXd robot_current_rotation(3,3); 		//
MatrixXd hd_del_rotation(3,3); 		//
MatrixXd hd_current_rotation(3,3); 		//
MatrixXd hd_initial_rotation(3,3); 		//
MatrixXd Euler_angle(3,3); 		//
MatrixXd Euler_x(3,3); 		//
MatrixXd Euler_y(3,3); 		//
MatrixXd Euler_z(3,3); 		//

char ReadKey;
//*********************************HAMMERING TASK DEFINITION******************************************
double phi;					//Rotate angle along z axis from EE destination pos - to x axis
double alpha;				//Rotate angle along z axis from EE destination pos - to x axis
double inz;					//Incremental/Decremental value along z
double psi_ham;				//Arm angle
double SI_del = 1;		    //Arm angle in teleoperation mode
MatrixXd rotm(3,3);			//Intermediate rotation matrix
MatrixXd p_e(3,1);			//Given EE destination postion
MatrixXd p_save(3,1);		//Saving EE destination postion
MatrixXd p(3,1);			//computed EE destination postion
MatrixXd p_temp1(3,1);		//Temp1 pos for computing
MatrixXd p_temp2(3,1);		//Temp2 pos for computing
MatrixXd P_HAM(4,4);		//EE homogeneous transformation matrix

//**************************************PMAC DEFINITION***********************************************
#define selected_PLC	8
int motor[7];                           //motor index
double Kt[7];                           //motor torque constant (1/A)
double Nhd[7];                          //Harmonic driver's gear ratio
double Icon[7];                         //motor driver's continuous current (A)
double Kp[7], Kd[7];	                //P gain for joint i
double T_limit[7];	                    //Torque limit (for safety)
double fric_amp_lim[7];                 //Friction compensate current (A) limit +/-

double const dt_pmac=0.1;	            //PMAC operation frequency (s)
double const dtheta_threshold=0.008;	//friction compensate torque: joinl vel threshold (rad/s)

std::string reply;                      //reply message from each PMAC command
int MotorBreakOff[7];                   //Checking motor break off sequence
int AllBreakOff;                        // =0 if all motors were broken off
int VelUpdate[7];                       //Check updating motor velocies status
int AllVelUpt = 0;                      // =0  if all motor's vel were updated

std::string breakoff1 = "#1j/";         //motor driver's break off/on command
std::string breakoff2 = "#2j/";
std::string breakoff3 = "#3j/";
std::string breakoff4 = "#4j/";
std::string breakoff5 = "#5j/";
std::string breakoff6 = "#6j/";
std::string breakoff7 = "#7j/";
std::string breakon1 = "#1k";
std::string breakon2 = "#2k";
std::string breakon3 = "#3k";
std::string breakon4 = "#4k";
std::string breakon5 = "#5k";
std::string breakon6 = "#6k";
std::string breakon7 = "#7k";
//std::string rot6_90 = "#6j=90000";
//std::string rot7_30 = "#7j=30000";

std::string ECATEn = "ECAT[0].enable=1";	        //Enable EtherCAT message
std::string ECATDs = "ECAT[0].enable=0";	        //Disable EtherCAT message

std::string ECATReg5 = "ECAT[0].IO[5].Data=8";		//Position mode setup for etherCAT
std::string ECATReg12 = "ECAT[0].IO[12].Data=8";	//Position mode setup for etherCAT
std::string ECATReg19 = "ECAT[0].IO[19].Data=8";	//Position mode setup for etherCAT
std::string ECATReg26 = "ECAT[0].IO[26].Data=8";	//Position mode setup for etherCAT
std::string ECATReg33 = "ECAT[0].IO[33].Data=8";	//Position mode setup for etherCAT
std::string ECATReg40 = "ECAT[0].IO[40].Data=8";	//Position mode setup for etherCAT
std::string ECATReg47 = "ECAT[0].IO[47].Data=8";	//Position mode setup for etherCAT

std::string u_ipaddr 	= "192.168.0.200";          //PMAC IP address
std::string u_user 	= "root";
std::string u_passw	= "deltatau";
std::string u_port	= "22";
bool u_nominus2	= false;
int runfirst = 10;

PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();   //create a new PMAC pointer

//**************************************GLOBAL PARAMETERS*********************************************
//ROSE nodes description:
ros::Publisher pub_jointp;	                        //joint position publisher node
ros::Publisher pub_jointt;	                        //joint torque publisher node
ros::Subscriber hd_trans;                           //sub transformation matrix
ros::Subscriber hd_buttons;                         //sub phantom omni button
ros::Subscriber hd_char;	                        //sub keyboard
ros::Subscriber sub_jointp;	                        //joint position subcriber node
ros::Subscriber sub_jointv;	                        //joint velocity subcriber node
ros::Subscriber sub_jointt;	                        //joint torque subcriber node


double	current_joint_states[7];	                //current joint angles _ from  msgCallbackP function
double	cur_vel[7];			                        //current joint vel _ from  msgCallbackv function
double	cur_tau[7];			                        //current joint torque _ from  msgCallbackT function
double	robot_current_xyz[3];;		                //current pos - output of forward kinematics

double SI,SI_COM,SI_HIG,SI_LOW,SI_FIX;              //Computed arm angles - redundancy
double q[7], q_pmac[7], th[7], q_pmac_update[7], dq_pmac_update[7]; //Imtermediate joint angles for a specific computational
const int N = 1000;		                            //Number of Cartesian steps
double	dt;			                                //incremental step time

//********************************Eigen Matrices Definition***********************************
//Given
MatrixXd P_INIT(4,4);	            //Given init homogeneous transformation matrix
MatrixXd P_END(4,4);	            //Given end homogeneous transformation matrix
MatrixXd P_COM(4,4);	            //Given end homogeneous transformation matrix

//Temp matrices for Inverse Kinematics computation
MatrixXd W(3,1); 		            //Wrist position relating to base
MatrixXd DZ(3,1);		            //[0; 0; 1] matrix
MatrixXd L_SW(3,1); 	            //Wrist position relating to shoulder
MatrixXd D1(3,1); 		            //D1 column matrix
MatrixXd KSW(3,3); 		            //Skew symetric matrix of vector between wrist and shoulder
MatrixXd USW(3,1); 		            //Unit vector of L_SW
MatrixXd R03dot(3,3); 		        //Rotation matrix of the ref joint angles q1dot, q2dot
MatrixXd XS(3,3); 		            //Shoulder constant matrix
MatrixXd YS(3,3); 		            //
MatrixXd ZS(3,3); 		            //
MatrixXd I(3,3); 		            //Identity matrix
MatrixXd I7(7,7); 		            //Identity matrix 7x7
MatrixXd R34(3,3); 		            //Rotation matrix from joint 3 to joint 4
MatrixXd XW(3,3); 		            //Wrist constant matrix
MatrixXd YW(3,3); 		            //
MatrixXd ZW(3,3); 		            //
//Temp matrices for stiffness control computation
MatrixXd desired_theta(7,1);	    //Desired joint positions
MatrixXd desired_dtheta(7,1);	    //Desired joint velocities
MatrixXd current_theta(7,1);	    //Current joint position
MatrixXd current_dtheta(7,1);	    //Current joint velocities
MatrixXd theta_err1(7,1);	        //Joint position error 1
MatrixXd theta_err2(7,1);	        //Joint position error 2
MatrixXd dtheta_err(7,1);	        //Joint velocity error

MatrixXd Jacob(6,7);		        //Jacobian matrix 6x7
MatrixXd Trans_Jacob(7,6);	        //Jacobian transpose matrix 7x6
MatrixXd pinv_Jacob(7,6);	        //Jacobian pseudo inverse matrix 7x6

MatrixXd Kx(6,6); 	                //cartesian stiffness diagonal matrix
MatrixXd KxVec(6,1); 	            //stiffness diagonal vector
MatrixXd Ki(7,7); 	                //joint stiffness matrix
MatrixXd Kmin(7,7); 	            //minimum joint stiffness matrix under mapping Kc -> Ki
MatrixXd Knull(7,7); 	            //Null space joint stiffness under mapping Kc -> Ki
MatrixXd K0(7,7); 	                //weighting matrix (symmetric positive definite) under mapping Kc -> Ki
MatrixXd K0Vec(7,1); 	            //weighting matrix diagonal vector

MatrixXd Dx(6,6); 	                //cartesian damping diagonal matrix
MatrixXd DxVec(6,1); 	            //damping diagonal vector
MatrixXd Di(7,7); 	                //joint damping matrix

MatrixXd Tau_stif(7,1);		        //Torque computed from stiffness control
MatrixXd Friction_I(7,1);	        //Friction compensation torque
MatrixXd Gravity_T(7,1);	        //Gravity compensation torque
MatrixXd Tau_I(7,1);		        //Overall torque -> send to PMAC

//*****************************************SUB PROGRAMS***********************************************

//***************************************Delay ms function********************************************
void delay_ms(int count)
{
	int i,j;
	for (i=0; i<count; i++)
	{
		for (j=0; j<1000; j++)
		{
		}
	}
}

//****************************************sign function***********************************************

int sgn(double x)
{
	if (x > 0) return 1;
	if (x < 0) return -1;
	if (x == 0) return 0;
}

//*****************************Call Back Trans matrix from Master device*******************************
void hd_callback_trans(const downscale4::trans::ConstPtr& msg) {                                      
	
		Output_data[0] = msg->a;
		Output_data[1] = msg->b;
		Output_data[2] = msg->c;
		Output_data[3] = msg->d;
		Output_data[4] = msg->e;
		Output_data[5] = msg->f;
		Output_data[6] = msg->g;
		Output_data[7] = msg->h;
		Output_data[8] = msg->i;
		Output_data[9] = msg->j;
		Output_data[10] = msg->k;
		Output_data[11] = msg->l;
		Output_data[12] = msg->m;
		Output_data[13] = msg->n;
		Output_data[14] = msg->o;
		Output_data[15] = msg->p;

		hd_rotation_matrix[0] = -Output_data[2];
		hd_rotation_matrix[1] = -Output_data[0];
		hd_rotation_matrix[2] = Output_data[1];
		hd_rotation_matrix[3] = -Output_data[6];
		hd_rotation_matrix[4] = -Output_data[4];
		hd_rotation_matrix[5] = Output_data[5];
		hd_rotation_matrix[6] = -Output_data[10];
		hd_rotation_matrix[7] = -Output_data[8];
		hd_rotation_matrix[8] = Output_data[9];

		hd_current_xyz[0] = Output_data[12];
		hd_current_xyz[1] = Output_data[13];
		hd_current_xyz[2] = Output_data[14];
}

//********************************Call Back Buttons from Master device*******************************
void hd_callback_buttons(const downscale4::buttons::ConstPtr& msga) {

	Buttons[0] = msga->a;
	Buttons[1] = msga->b;
	Inkwell    = msga->c;

	//std::cout << std::string(80, '-') << std::endl;
}

//**************************************Call Back keyboard********************************************
void hd_callback_keyboards(const downscale4::charmsg::ConstPtr& msga) {

	ReadKey = msga->a;
	//printf(" %c \n",ReadKey);

}
//**********************************Call Back Joint Position******************************************
void msgCallbackP(const downscale4::seven::ConstPtr& msg)
{

	current_joint_states[0] = msg->a;
	current_joint_states[1] = msg->b;
	current_joint_states[2] = msg->c;
	current_joint_states[3] = msg->d;
	current_joint_states[4] = msg->e;
	current_joint_states[5] = msg->f;
	current_joint_states[6] = msg->g;

}

//***********************************Call Back Joint Torque*******************************************
void msgCallbackT(const downscale4::seven::ConstPtr& msg)
{

	cur_tau[0] = msg->a;
	cur_tau[1] = msg->b;
	cur_tau[2] = msg->c;
	cur_tau[3] = msg->d;
	cur_tau[4] = msg->e;
	cur_tau[5] = msg->f;
	cur_tau[6] = msg->g;


}

//*********************************Call Back Joint Velocities*****************************************
void msgCallbackv(const downscale4::seven::ConstPtr& msg)
{

	cur_vel[0] = msg->a;
	cur_vel[1] = msg->b;
	cur_vel[2] = msg->c;
	cur_vel[3] = msg->d;
	cur_vel[4] = msg->e;
	cur_vel[5] = msg->f;
	cur_vel[6] = msg->g;
}

//**********************************Joint position publisher******************************************
void joint_publish(double theta[7])
{
	downscale4::seven msgp;
	msgp.a = theta[0];
	msgp.b = theta[1];
	msgp.c = theta[2];
	msgp.d = theta[3];
	msgp.e = theta[4];
	msgp.f = theta[5];
	msgp.g = theta[6];

	pub_jointp.publish(msgp);
}

//************************************Joint torque publisher*******************************************
void jointt_publish(double tau[7])
{
	downscale4::seven msgp;
	msgp.a = tau[0];
	msgp.b = tau[1];
	msgp.c = tau[2];
	msgp.d = tau[3];
	msgp.e = tau[4];
	msgp.f = tau[5];
	msgp.g = tau[6];

	pub_jointt.publish(msgp);

}

//*******************************Inverse Kinematics***************************************************
void InvK7(const Eigen::MatrixXd &data, double si)
{

	double q1dot, q2dot;
	double norm_L_SW;
	int GC2, GC4, GC6;
	//Given homogeneous transformation matrix

	DZ << 	0, 0, 1;
	D1 << 	0, 0, d1;
	I = I.setIdentity(3,3);

	//Compute wrist position relating to base from the given EE position end orientation
	W = data.topRightCorner(3,1)-d7*(data.topLeftCorner(3,3)*DZ);	//P(1:3,4)
	//Compute wrist position relating to shoulder
	L_SW = W - D1;
	norm_L_SW = L_SW.norm();
	//Elbow angle q4 in radian
	q[3] = acos((pow(norm_L_SW,2) - pow(d3,2) - pow(d5,2))/(2*d3*d5));
	if (q[3]>=0) GC4 = 1;
	else GC4 = -1;
	//Compute skew symetric matrix of the vector between wrist and shoulder:
	USW = L_SW/norm_L_SW;
	KSW << 	0, -USW(2), USW(1),
			USW(2), 0, -USW(0),
			-USW(1), USW(0), 0;
	//Set q3=0, compute reference joint angle q1dot, q2dot of the ref plane
	q1dot = atan2(W(1),W(0));
	q2dot = pi/2 - asin((W(2)-d1)/norm_L_SW) - acos((pow(d3,2)+pow(norm_L_SW,2)-pow(d5,2))/(2*d3*norm_L_SW));
	//Rotation matrix of the ref joint angles q1dot, q2dot:
	R03dot << 	cos(q1dot)*cos(q2dot),	-cos(q1dot)*sin(q2dot),	-sin(q1dot),
				cos(q2dot)*sin(q1dot),	-sin(q1dot)*sin(q2dot),	cos(q1dot),
				-sin(q2dot),			-cos(q2dot),			0;
	//Shoulder constant matrices Xs Ys Zs
	XS = KSW*R03dot;
	YS = -(KSW*KSW)*R03dot;
	ZS = (I+KSW*KSW)*R03dot;
	//constant matrices Xw Yw Zw
	R34 << 		cos(q[3]),		0,		sin(q[3]),
				sin(q[3]),		0, 		-cos(q[3]),
				0,				1,		0;
	XW = R34.transpose()*XS.transpose()*data.topLeftCorner(3,3);
	YW = R34.transpose()*YS.transpose()*data.topLeftCorner(3,3);
	ZW = R34.transpose()*ZS.transpose()*data.topLeftCorner(3,3);

	//Theta2
	q[1] = acos(-sin(si)*XS(2,1)-cos(si)*YS(2,1)-ZS(2,1));
	if (q[1]>=0)GC2 = 1;
	else GC2 = -1;
	//Theta1, theta3
	q[0] = atan2(GC2*(-sin(si)*XS(1,1)-cos(si)*YS(1,1)-ZS(1,1)),GC2*(-sin(si)*XS(0,1)-cos(si)*YS(0,1)-ZS(0,1)));
	q[2] = atan2(GC2*(sin(si)*XS(2,2)+cos(si)*YS(2,2)+ZS(2,2)),GC2*(-sin(si)*XS(2,0)-cos(si)*YS(2,0)-ZS(2,0)));

	//Theta6
	q[5] = acos(sin(si)*XW(2,2)+cos(si)*YW(2,2)+ZW(2,2));
	if (q[5]>=0)GC6 = 1;
	else GC6 = -1;
	//Theta5, theta7
	q[4] = atan2(GC6*(sin(si)*XW(1,2)+cos(si)*YW(1,2)+ZW(1,2)),GC6*(sin(si)*XW(0,2)+cos(si)*YW(0,2)+ZW(0,2)));
	q[6] = atan2(GC6*(sin(si)*XW(2,1)+cos(si)*YW(2,1)+ZW(2,1)),GC6*(-sin(si)*XW(2,0)-cos(si)*YW(2,0)-ZW(2,0)));

}

//***********************************Forward Kinematics***********************************************
MatrixXd forwd7(double qdata[7])
{
	MatrixXd pforwk7(3,1);			//computed EE destination postion
	double th1, th2, th3, th4, th5, th6, th7;

	//Update theta from msgCallbackP function
	th1 = qdata[0];
	th2 = qdata[1];
	th3 = qdata[2];
	th4 = qdata[3];
	th5 = qdata[4];
	th6 = qdata[5];
	th7 = qdata[6];

	pforwk7(0,0) = d3 * cos(th1)*sin(th2) - d7 * (cos(th6)*(sin(th4)*(sin(th1)*sin(th3) - cos(th1)*cos(th2)*cos(th3)) - cos(th1)*cos(th4)*sin(th2)) + sin(th6)*(cos(th5)*(cos(th4)*(sin(th1)*sin(th3) - cos(th1)*cos(th2)*cos(th3)) + cos(th1)*sin(th2)*sin(th4)) + sin(th5)*(cos(th3)*sin(th1) + cos(th1)*cos(th2)*sin(th3)))) - d5 * (sin(th4)*(sin(th1)*sin(th3) - cos(th1)*cos(th2)*cos(th3)) - cos(th1)*cos(th4)*sin(th2));

	pforwk7(1,0) = d5 * (sin(th4)*(cos(th1)*sin(th3) + cos(th2)*cos(th3)*sin(th1)) + cos(th4)*sin(th1)*sin(th2)) + d7 * (cos(th6)*(sin(th4)*(cos(th1)*sin(th3) + cos(th2)*cos(th3)*sin(th1)) + cos(th4)*sin(th1)*sin(th2)) + sin(th6)*(cos(th5)*(cos(th4)*(cos(th1)*sin(th3) + cos(th2)*cos(th3)*sin(th1)) - sin(th1)*sin(th2)*sin(th4)) + sin(th5)*(cos(th1)*cos(th3) - cos(th2)*sin(th1)*sin(th3)))) + d3 * sin(th1)*sin(th2);

	pforwk7(2,0) = d1 - d7 * (sin(th6)*(cos(th5)*(cos(th2)*sin(th4) + cos(th3)*cos(th4)*sin(th2)) - sin(th2)*sin(th3)*sin(th5)) - cos(th6)*(cos(th2)*cos(th4) - cos(th3)*sin(th2)*sin(th4))) + d5 * (cos(th2)*cos(th4) - cos(th3)*sin(th2)*sin(th4)) + d3 * cos(th2);

	return pforwk7;
}

//************************************Rotation matrices***********************************************
MatrixXd rotx(double angle)
{
	MatrixXd data(3,3);
	data(0,0) = 1;
	data(0,1) = 0;
	data(0,2) = 0;
	data(1,0) = 0;
	data(1,1) = cos(angle);
	data(1,2) = -sin(angle);
	data(2,0) = 0;
	data(2,1) = sin(angle);
	data(2,2) = cos(angle);
	return data;
}

MatrixXd roty(double angle)
{
	MatrixXd data(3,3);
	data(0,0) = cos(angle);
	data(0,1) = 0;
	data(0,2) = sin(angle);
	data(1,0) = 0;
	data(1,1) = 1;
	data(1,2) = 0;
	data(2,0) = -sin(angle);
	data(2,1) = 0;
	data(2,2) = cos(angle);
	return data;
}

MatrixXd rotz(double angle)
{
	MatrixXd data(3,3);
	data(0,0) = cos(angle);
	data(0,1) = -sin(angle);
	data(0,2) = 0;
	data(1,0) = sin(angle);
	data(1,1) = cos(angle);
	data(1,2) = 0;
	data(2,0) = 0;
	data(2,1) = 0;
	data(2,2) = 1;
	return data;
}

//*****************************Compute Cartesian Trajectory*******************************************
void calc_cartezian_trajectory(double CT[3][N], double ini[3], double des[3])
{

	double trajectory_length;		//the length from P0 -> P1
	double trajectory_dir_vec[3];	//trajectory direction vector
	double v_max;					//maximum velocity
	double t=0; 					//incremental running time
	double step_size[N];			//counting step
	int i=1;


	double dt, duration;

	dt	= 0.01;						//control command period
	duration = dt * N;				//total time

	trajectory_length = sqrt(pow((des[0] - ini[0]),2) + pow((des[1] - ini[1]),2) + pow((des[2] - ini[2]),2));

	//ROS_INFO("trajectory length: %f", trajectory_length);

	trajectory_dir_vec[0] = (des[0] - ini[0]) / trajectory_length;
    std::string ECATReg5 = "ECAT[0].IO[5].Data=8";		//Position mode setup for etherCAT
    std::string ECATReg12 = "ECAT[0].IO[12].Data=8";	//Position mode setup for etherCAT
    std::string ECATReg19 = "ECAT[0].IO[19].Data=8";	//Position mode setup for etherCAT
    std::string ECATReg26 = "ECAT[0].IO[26].Data=8";	//Position mode setup for etherCAT
    std::string ECATReg33 = "ECAT[0].IO[33].Data=8";	//Position mode setup for etherCAT
    std::string ECATReg40 = "ECAT[0].IO[40].Data=8";	//Position mode setup for etherCAT
    std::string ECATReg47 = "ECAT[0].IO[47].Data=8";	//Position mode setup for etherCAT	trajectory_dir_vec[1] = (des[1] - ini[1]) / trajectory_length;
	trajectory_dir_vec[2] = (des[2] - ini[2]) / trajectory_length;

	//ROS_INFO("trajectory x direction: %f",trajectory_dir_vec[0]);
	//ROS_INFO("trajectory y direction: %f",trajectory_dir_vec[1]);
	//ROS_INFO("trajectory z direction: %f",trajectory_dir_vec[2]);

	v_max = (4 * trajectory_length)/(3 * duration);

	//ROS_INFO("max velocity: %f",v_max);

	//the trajectory divided to three different parts.
	//1st part is quarter of the duration time that have acceleration.
	//2nd part is two quarter of the duration time with steady velocity.
	//3rd part is another quarter of the duration time that have deceleration.
	step_size[0] = (2 * v_max / duration) * pow(dt,2);		//acceleration = 4*v_max/duration (1/2 at^2)
	t+=dt;
	while (t < duration / 4) //1st part
	{
		step_size[i] = (4 * v_max /duration) * t * dt + (2 * v_max /duration) * pow(dt,2);
		i++;
		t+=dt;
	}
	while (t < 3 * duration / 4) //2nd part
	{
		step_size[i] = v_max * dt;
		i++;
		t+=dt;
	}
	while (t <= duration) //3rd part
	{
		step_size[i] = (4 * v_max - 4 / duration * v_max * t) * dt - 2 * v_max / duration * pow(dt,2);
		i++;
		t+=dt;
	}

	//calculating the cartesian trajectory
	for (i=0; i<3; i++)
	{
		CT[i][0] = step_size[0] * trajectory_dir_vec[i] + ini[i];
	}
	//ROS_INFO("cartesian step #1 : [%f , %f , %f]",CT[0][0],CT[1][0],CT[2][0]);
	//ROS_INFO("");
	for (i=1; i<N ; i++)
	{
		for (int j=0; j<3 ; j++)
		{
			CT[j][i] = step_size[i] * trajectory_dir_vec[j] + CT[j][i-1];
		}
		//ROS_INFO("cartezian step #%d : [%f , %f , %f]",i+1,CT[0][i],CT[1][i],CT[2][i]);
	}

}

//***********************************PMAC CONTROL FUNCTIONS*******************************************
//*************************************Disable PLC - PMAC*********************************************
void disablePLC(PowerPMACcontrol *ppmaccomm, int PLCnum)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	ppmaccomm->PowerPMACcontrol_disablePlc(PLCnum);
}

//*************************************Enable PLC - PMAC*********************************************
void enablePLC(PowerPMACcontrol *ppmaccomm, int PLCnum)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int isEnable = ppmaccomm->PowerPMACcontrol_disablePlc(PLCnum);
	if (isEnable != 0)
		{
		printf("Error in openning PLC. exit:\n");
		}
		else
		{
			printf("PLC is openned.\n");
		}
}

//***********************************Enable EtherCAT - PMAC*******************************************
void etherCAT_En(PowerPMACcontrol *ppmaccomm)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int etherCATIsEn = ppmaccomm->PowerPMACcontrol_sendCommand(ECATEn, reply);
	if (etherCATIsEn != 0)
		{
		printf("EtherCAT Enable error. exit:\n");
		}
		else
		{
			printf("EtherCAT is enable.\n");

		}
}

//***********************************Disable EtherCAT - PMAC******************************************
void etherCAT_Ds(PowerPMACcontrol *ppmaccomm)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int etherCATIsDs = ppmaccomm->PowerPMACcontrol_sendCommand(ECATDs, reply);
	if (etherCATIsDs != 0)
		{
		printf("EtherCAT Disable error. exit:\n");
		}
		else
		{
			printf("EtherCAT is disable.\n");
			//printf("respond: %s \n",&reply);

		}
}

//**************************Send PMAC Command to Stop All Motors**************************************
void stopallmotor(PowerPMACcontrol *ppmaccomm)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsBrkon[7], sum;
	IsBrkon[0] = ppmaccomm->PowerPMACcontrol_axisAbort(motor[0]);
	IsBrkon[1] = ppmaccomm->PowerPMACcontrol_axisAbort(motor[1]);
	IsBrkon[2] = ppmaccomm->PowerPMACcontrol_axisAbort(motor[2]);
	IsBrkon[3] = ppmaccomm->PowerPMACcontrol_axisAbort(motor[3]);
	IsBrkon[4] = ppmaccomm->PowerPMACcontrol_axisAbort(motor[4]);
	IsBrkon[5] = ppmaccomm->PowerPMACcontrol_axisAbort(motor[5]);
	IsBrkon[6] = ppmaccomm->PowerPMACcontrol_axisAbort(motor[6]);	
	sum = IsBrkon[0]+IsBrkon[1]+IsBrkon[2]+IsBrkon[3]+IsBrkon[4]+IsBrkon[5]+IsBrkon[6];
	if(sum != 0) 	printf("breakon error. exit:\n");
	else 		printf("All motors are arborted.\n");
}

//************************Send PMAC Command to Break off All Motors***********************************
void onMotorRelays(PowerPMACcontrol *ppmaccomm)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsBrkoff[7], sum;
	/*IsBrkoff[0] = ppmaccomm->PowerPMACcontrol_axisStop(motor[0]);
	delay_ms(500);
	IsBrkoff[1] = ppmaccomm->PowerPMACcontrol_axisStop(motor[1]);
	delay_ms(500);
	IsBrkoff[2] = ppmaccomm->PowerPMACcontrol_axisStop(motor[2]);
	sdelay_ms(500);
	IsBrkoff[3] = ppmaccomm->PowerPMACcontrol_axisStop(motor[3]);
	sdelay_ms(500);
	IsBrkoff[4] = ppmaccomm->PowerPMACcontrol_axisStop(motor[4]);
	delay_ms(500);*/
	IsBrkoff[5] = ppmaccomm->PowerPMACcontrol_axisStop(motor[5]);
	delay_ms(500);
	IsBrkoff[6] = ppmaccomm->PowerPMACcontrol_axisStop(motor[6]);
	delay_ms(500);
	sum = IsBrkoff[0]+IsBrkoff[1]+IsBrkoff[2]+IsBrkoff[3]+IsBrkoff[4]+IsBrkoff[5]+IsBrkoff[6];
	if(sum != 0) 	printf("break-off error. exit:\n");
	else 		printf("All motor's relays are activated.\n");
}

//*************************************Break off Torque Mode******************************************
void breakofftorquemode(PowerPMACcontrol *ppmaccomm, int axis)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	std::string brkofftorque;
	brkofftorque = "#" + std::to_string(axis) + "out0\n";
	int Isbrkofftorque = ppmaccomm->PowerPMACcontrol_sendCommand(brkofftorque, reply);
	if (Isbrkofftorque != 0)
	{
		printf("Break off Joint [%d] error.\n", axis);
	}
	else
	{
		printf("Break off Joint [%d] in torque mode.\n", axis);

	}
}

//*************************************Send torque command******************************************
void sendtorquecommand(PowerPMACcontrol *ppmaccomm, int axis, double command)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	std::string torquecom;
	torquecom = "Motor[" + std::to_string(axis) + "].CompDac=" + std::to_string(command) + "\n";
	int Istorquecom = ppmaccomm->PowerPMACcontrol_sendCommand(torquecom, reply);
	if (Istorquecom != 0)
	{
		printf("Joint [%d] torque command error.\n", axis);
	}
	else
	{
		printf("Joint [%d] torque = %f.\n", axis, command);

	}
}

//***************************************Friction compensate******************************************
double frictioncomptorque(int axis, double dq)
{
	//input dq [rad/s] -> output Friction_I current [A]	
	double Af, Bf, Cf, Df, Friction_I;
	//A, B, C, D are fitting coefficients of friction model
	//F = A*sgn(dq) + B*dq + C*(1-exp(-dq/D))
	
	if (axis == 0)
	{
		if (dq>=0)
		{
			//positive velocity case:
			Af = 0.2405;
			Bf = -0.4581;
			Cf = 0.1674;
			Df = 0.1287;
		}
		if (dq<0)
		{
			//negative velocity case:
			Af = 0.2252;
			Bf = 14.8581;
			Cf = -52.9605;
			Df = 4.0048;
		}	
	}
	if (axis == 1)
	{
		if (dq>=0)
		{
			//positive velocity case:
			Af = 0.4115;
			Bf = 1.5737;
			Cf = 0.2083;
			Df = 0.0861;
		}
		if (dq<0)
		{
			//negative velocity case:
			Af = 0.44;
			Bf = 10.533;
			Cf = -19.963;
			Df = 2.555;
		}	
	}
	if (axis == 2)
	{
		if (dq>=0)
		{
			//positive velocity case:
			Af = 0.3346;
			Bf = -3.5017;
			Cf = 18.095;
			Df = 3.6844;
		}
		if (dq<0)
		{
			//negative velocity case:
			Af = 0.3274;
			Bf = 2.1717;
			Cf = -5.0523;
			Df = 5.0954;
		}	
	}
	if (axis == 3)
	{
		if (dq>=0)
		{
			//positive velocity case:
			Af = 0.2243;
			Bf = 2.0852;
			Cf = -5.3291;
			Df = 4.608;
		}
		if (dq<0)
		{
			//negative velocity case:
			Af = 0.246;
			Bf = 0.841;
			Cf = -0.088;
			Df = -0.155;
		}	
	}
	if (axis == 4)
	{
		if (dq>=0)
		{
			//positive velocity case:
			Af = -0.0735;
			Bf = 0.7954;
			Cf = 0.3168;
			Df = 0.0000001;
		}
		if (dq<0)
		{
			//negative velocity case:
			Af = 0.131;
			Bf = 0.696;
			Cf = -0.142;
			Df = -0.002;
		}	
	}	
	if (axis == 5)
	{
		if (dq>=0)
		{
			//positive velocity case:
			Af = 0.174;
			Bf = 0.553;
			Cf = 0.054;
			Df = 0.053;
		}
		if (dq<0)
		{
			//negative velocity case:
			Af = 0.18;
			Bf = -0.115;
			Cf = -0.794;
			Df = -0.819;
		}	
	}
	if (axis == 6)
	{
		if (dq>=0)
		{
			//positive velocity case:
			Af = 0.172;
			Bf = 0.137;
			Cf = 0.048;
			Df = 0.144;
		}
		if (dq<0)
		{
			//negative velocity case:
			Af = 0.166;
			Bf = 5.087;
			Cf = -56.387;
			Df = 11.932;
		}	
	}

	printf("Af %.5f Bf  %.5f Cf %.5f Df %.5f \n", Af, Bf, Cf, Df);
	
	//Friction torque [Nm]
	if (dq>0) Friction_I = Af + Bf*dq + Cf*(1-exp(-dq/Df));
	if (dq<0) Friction_I = -Af + Bf*dq + Cf*(1-exp(-dq/Df));
	if (dq == 0) Friction_I = Bf*dq + Cf*(1-exp(-dq/Df));
	//Friction_I = A*sgn(dq) + B*dq + C*(1-exp(-dq/D));
	
	if (axis == 4) Friction_I = 0.8*Friction_I;
	if (axis == 1) Friction_I = 0.8*Friction_I;
	printf("t1 %.5f t2  %.5f t3 %.5f dq %.5f Friction com %.5f \n", Af, Bf*dq, Cf*(1-exp(-dq/Df)), dq, Friction_I);
	return Friction_I;
}

//***************************************Gravity compensate******************************************
double gravitycomptorque(int axis, double q[7])
{
	double Gravity_T;
	if (axis == 0)
	{
		//Joint 1 = 0 most cases			
		Gravity_T = 0;
	}	
	if (axis == 1)
	{
		//Joint 2			
		Gravity_T = - g_acc*m[6]*(d5*(cos(q[3])*sin(q[1]) + cos(q[1])*cos(q[2])*sin(q[3])) + d3*sin(q[1]) - lg7*(sin(q[5])*(cos(q[4])*(sin(q[1])*sin(q[3]) - cos(q[1])*cos(q[2])*cos(q[3])) + cos(q[1])*sin(q[2])*sin(q[4])) - cos(q[5])*(cos(q[3])*sin(q[1]) + cos(q[1])*cos(q[2])*sin(q[3])))) - g_acc*m[4]*(lg5*(cos(q[3])*sin(q[1]) + cos(q[1])*cos(q[2])*sin(q[3])) + d3*sin(q[1])) - g_acc*m[5]*(lg6*(cos(q[3])*sin(q[1]) + cos(q[1])*cos(q[2])*sin(q[3])) + d3*sin(q[1])) - g_acc*lg3*m[2]*sin(q[1]) - g_acc*lg4*m[3]*sin(q[1]);
	}
	if (axis == 2)
	{
		//Joint 3 = 0 most cases			
		Gravity_T = g_acc*sin(q[1])*(lg5*m[4]*sin(q[2])*sin(q[3]) + lg6*m[5]*sin(q[2])*sin(q[3]) + d5*m[6]*sin(q[2])*sin(q[3]) + lg7*m[6]*cos(q[5])*sin(q[2])*sin(q[3]) + lg7*m[6]*cos(q[2])*sin(q[4])*sin(q[5]) + lg7*m[6]*cos(q[3])*cos(q[4])*sin(q[2])*sin(q[5]));
	}
	if (axis == 3)
	{
		//Joint 4			
		Gravity_T = - g_acc*m[6]*(d5*(cos(q[1])*sin(q[3]) + cos(q[2])*cos(q[3])*sin(q[1])) + lg7*(cos(q[5])*(cos(q[1])*sin(q[3]) + cos(q[2])*cos(q[3])*sin(q[1])) + cos(q[4])*sin(q[5])*(cos(q[1])*cos(q[3]) - cos(q[2])*sin(q[1])*sin(q[3])))) - g_acc*lg5*m[4]*(cos(q[1])*sin(q[3]) + cos(q[2])*cos(q[3])*sin(q[1])) - g_acc*lg6*m[5]*(cos(q[1])*sin(q[3]) + cos(q[2])*cos(q[3])*sin(q[1]));
	}
	if (axis == 4)
	{
		//Joint 5 = 0 most cases			
		Gravity_T = g_acc*lg7*m[6]*sin(q[5])*(sin(q[4])*(cos(q[1])*sin(q[3]) + cos(q[2])*cos(q[3])*sin(q[1])) + cos(q[4])*sin(q[1])*sin(q[2]));
	}
	if (axis == 5)
	{
		//Joint 6			
		Gravity_T = - g_acc*lg7*m[6]*(cos(q[5])*(cos(q[4])*(cos(q[1])*sin(q[3]) + cos(q[2])*cos(q[3])*sin(q[1])) - sin(q[1])*sin(q[2])*sin(q[4])) + sin(q[5])*(cos(q[1])*cos(q[3]) - cos(q[2])*sin(q[1])*sin(q[3])));
	}
	if (axis == 6)
	{
		//Joint 7 = 0 			
		Gravity_T = 0;
	}
	
	printf("Gravity compensate [%d] = %.5f \n", axis+1, Gravity_T);
	return Gravity_T;
	
}

//***********************************Set motor acceleration*******************************************
void motoraccset(PowerPMACcontrol *ppmaccomm, int acc1, int acc2, int acc3, int acc4, int acc5, int acc6, int acc7)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsAccSet[7], sum;
	IsAccSet[0] = ppmaccomm->PowerPMACcontrol_axisSetAcceleration(motor[0], acc1);
	IsAccSet[1] = ppmaccomm->PowerPMACcontrol_axisSetAcceleration(motor[1], acc2);
	IsAccSet[2] = ppmaccomm->PowerPMACcontrol_axisSetAcceleration(motor[2], acc3);
	IsAccSet[3] = ppmaccomm->PowerPMACcontrol_axisSetAcceleration(motor[3], acc4);
	IsAccSet[4] = ppmaccomm->PowerPMACcontrol_axisSetAcceleration(motor[4], acc5);
	IsAccSet[5] = ppmaccomm->PowerPMACcontrol_axisSetAcceleration(motor[5], acc6);
	IsAccSet[6] = ppmaccomm->PowerPMACcontrol_axisSetAcceleration(motor[6], acc7);
	sum = IsAccSet[0]+IsAccSet[1]+IsAccSet[2]+IsAccSet[3]+IsAccSet[4]+IsAccSet[5]+IsAccSet[6];
	if(sum != 0) 	printf("Motor Acceleration set error:\n");
	else 		printf("Motor[1-7]'s acceleration are set.\n");	
}

//*************************************Set motor velocity*********************************************
void motorvelset(PowerPMACcontrol *ppmaccomm, int vel1, int vel2, int vel3, int vel4, int vel5, int vel6, int vel7)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsVelSet[7], sum;
	IsVelSet[0] = ppmaccomm->PowerPMACcontrol_axisSetVelocity(motor[0], vel1);
	IsVelSet[1] = ppmaccomm->PowerPMACcontrol_axisSetVelocity(motor[1], vel2);
	IsVelSet[2] = ppmaccomm->PowerPMACcontrol_axisSetVelocity(motor[2], vel3);
	IsVelSet[3] = ppmaccomm->PowerPMACcontrol_axisSetVelocity(motor[3], vel4);
	IsVelSet[4] = ppmaccomm->PowerPMACcontrol_axisSetVelocity(motor[4], vel5);
	IsVelSet[5] = ppmaccomm->PowerPMACcontrol_axisSetVelocity(motor[5], vel6);
	IsVelSet[6] = ppmaccomm->PowerPMACcontrol_axisSetVelocity(motor[6], vel7);
	sum = IsVelSet[0]+IsVelSet[1]+IsVelSet[2]+IsVelSet[3]+IsVelSet[4]+IsVelSet[5]+IsVelSet[6];
	if(sum != 0) 	printf("Motor Velocities set error:\n");
	else 		printf("Motor[1-7]'s velocities are set.\n");	
}

//********************************Reset Robot to Home Position****************************************
void resetrobot(PowerPMACcontrol *ppmaccomm)
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsReset[7], sum;
	IsReset[0] = ppmaccomm->PowerPMACcontrol_axisHome(motor[0]);
	IsReset[1] = ppmaccomm->PowerPMACcontrol_axisHome(motor[1]);
	IsReset[2] = ppmaccomm->PowerPMACcontrol_axisHome(motor[2]);
	IsReset[3] = ppmaccomm->PowerPMACcontrol_axisHome(motor[3]);
	IsReset[4] = ppmaccomm->PowerPMACcontrol_axisHome(motor[4]);
	IsReset[5] = ppmaccomm->PowerPMACcontrol_axisHome(motor[5]);
	IsReset[6] = ppmaccomm->PowerPMACcontrol_axisHome(motor[6]);
	sum = IsReset[0]+IsReset[1]+IsReset[2]+IsReset[3]+IsReset[4]+IsReset[5]+IsReset[6];
	if(sum != 0) 	printf("Reseting error:\n");
	else 		printf("Robot was reset.\n");
}

//**************************************Move Arm to Angle*********************************************
void move_arm(PowerPMACcontrol *ppmaccomm, double theta_pmac[7])
{
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsMove[7], sum;
	IsMove[0] = ppmaccomm->PowerPMACcontrol_axisMoveAbs(motor[0], theta_pmac[0]);
	IsMove[1] = ppmaccomm->PowerPMACcontrol_axisMoveAbs(motor[1], theta_pmac[1]);
	IsMove[2] = ppmaccomm->PowerPMACcontrol_axisMoveAbs(motor[2], theta_pmac[2]);
	IsMove[3] = ppmaccomm->PowerPMACcontrol_axisMoveAbs(motor[3], theta_pmac[3]);
	IsMove[4] = ppmaccomm->PowerPMACcontrol_axisMoveAbs(motor[4], theta_pmac[4]);
	IsMove[5] = ppmaccomm->PowerPMACcontrol_axisMoveAbs(motor[5], theta_pmac[5]);
	IsMove[6] = ppmaccomm->PowerPMACcontrol_axisMoveAbs(motor[6], theta_pmac[6]);
	sum = IsMove[0]+IsMove[1]+IsMove[2]+IsMove[3]+IsMove[4]+IsMove[5]+IsMove[6];
	if(sum != 0) 	printf("Robot moving error:\n");
	else 		printf("Robot is moving.\n");
}

//***********************************Update current position******************************************
void currentconfig(PowerPMACcontrol *ppmaccomm, double q_pmac_udt[7])
{
	//return in radian	
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsGetPos[7], sum;
	IsGetPos[0] = ppmaccomm->PowerPMACcontrol_axisGetCurrentPosition(motor[0], q_pmac_udt[0]);
	IsGetPos[1] = ppmaccomm->PowerPMACcontrol_axisGetCurrentPosition(motor[1], q_pmac_udt[1]);
	IsGetPos[2] = ppmaccomm->PowerPMACcontrol_axisGetCurrentPosition(motor[2], q_pmac_udt[2]);
	IsGetPos[3] = ppmaccomm->PowerPMACcontrol_axisGetCurrentPosition(motor[3], q_pmac_udt[3]);
	IsGetPos[4] = ppmaccomm->PowerPMACcontrol_axisGetCurrentPosition(motor[4], q_pmac_udt[4]);
	IsGetPos[5] = ppmaccomm->PowerPMACcontrol_axisGetCurrentPosition(motor[5], q_pmac_udt[5]);
	IsGetPos[6] = ppmaccomm->PowerPMACcontrol_axisGetCurrentPosition(motor[6], q_pmac_udt[6]);
	sum = IsGetPos[0]+IsGetPos[1]+IsGetPos[2]+IsGetPos[3]+IsGetPos[4]+IsGetPos[5]+IsGetPos[6];
	if(sum != 0) 	printf("Position updating error:\n");
	else 		
	{	
		q_pmac_udt[0] = q_pmac_udt[0]*pi/180000;
		q_pmac_udt[1] = q_pmac_udt[1]*pi/180000;
		q_pmac_udt[2] = q_pmac_udt[2]*pi/180000;
		q_pmac_udt[3] = q_pmac_udt[3]*pi/180000;
		q_pmac_udt[4] = q_pmac_udt[4]*pi/180000;
		q_pmac_udt[5] = q_pmac_udt[5]*pi/180000;
		q_pmac_udt[6] = q_pmac_udt[6]*pi/180000;
	printf("Current pos %.2f %.2f %.2f %.2f %.2f %.2f %.2f.\n", q_pmac_udt[0], q_pmac_udt[1], q_pmac_udt[2], q_pmac_udt[3], q_pmac_udt[4], q_pmac_udt[5], q_pmac_udt[6]);
	}
}

//*********************************Update single joint velocity***************************************
int currentvel(PowerPMACcontrol *ppmaccomm, int axis, double& velocity)
{
	//PMAC return velocity value in [deg/s]
	char cmd[128] = "";
    	sprintf( cmd, "motor[%d].ActVel\n", axis);
	int ret = ppmaccomm->PowerPMACcontrol_sendCommand(cmd, reply);
	if (ret != 0)
		return ret;
	std::istringstream stream(reply);
	double dval;
	stream >> dval;
	if (stream.fail())
	{
	//failed to convert to double value
	return -1;
	}
	velocity = dval;
	printf("Current vel %.2f .\n", velocity);
	return 0;
	
}

//*******************************Update all current joint velocities**********************************
void allcurrentvel(PowerPMACcontrol *ppmaccomm, double dq_pmac_udt[7])
{
	//PMAC return velocity value in [deg/s]	
	//PowerPMACcontrol *ppmaccomm = new PowerPMACcontrol();
	int IsGetVel[7], sum;
	IsGetVel[0] = currentvel(ppmaccomm, motor[0], dq_pmac_udt[0]);
	IsGetVel[0] = currentvel(ppmaccomm, motor[1], dq_pmac_udt[1]);
	IsGetVel[0] = currentvel(ppmaccomm, motor[2], dq_pmac_udt[2]);
	IsGetVel[0] = currentvel(ppmaccomm, motor[3], dq_pmac_udt[3]);
	IsGetVel[0] = currentvel(ppmaccomm, motor[4], dq_pmac_udt[4]);
	IsGetVel[0] = currentvel(ppmaccomm, motor[5], dq_pmac_udt[5]);
	IsGetVel[0] = currentvel(ppmaccomm, motor[6], dq_pmac_udt[6]);
	sum = IsGetVel[0]+IsGetVel[1]+IsGetVel[2]+IsGetVel[3]+IsGetVel[4]+IsGetVel[5]+IsGetVel[6];
	if(sum != 0) 	printf("Velocity updating error:\n");
	else 		
	{	
		dq_pmac_udt[0] = dq_pmac_udt[0];	//deg/s  mdeg/s -> rad/s *pi/180000
		dq_pmac_udt[1] = dq_pmac_udt[1];
		dq_pmac_udt[2] = dq_pmac_udt[2];
		dq_pmac_udt[3] = dq_pmac_udt[3];
		dq_pmac_udt[4] = dq_pmac_udt[4];
		dq_pmac_udt[5] = dq_pmac_udt[5];
		dq_pmac_udt[6] = dq_pmac_udt[6];
	printf("Current vel %.2f %.2f %.2f %.2f %.2f %.2f %.2f.\n", dq_pmac_udt[0], dq_pmac_udt[1], dq_pmac_udt[2], dq_pmac_udt[3], dq_pmac_udt[4], dq_pmac_udt[5], dq_pmac_udt[6]);
	}
}

//*********************************Handle Signal for Stop********************************************
//*****Reconnect to PMAC, break on all motors, disconnect etherCAT, disable PLC, disconnect PMAC
void signal_handler(int signum = 0)
{
	/*	
	std::string ECATDs1 = "ECAT[0].enable=0";
	std::string brkon1 = "#1k";
	std::string brkon2 = "#2k";
	std::string brkon3 = "#3k";
	std::string brkon4 = "#4k";
	std::string brkon5 = "#5k";
	std::string brkon6 = "#6k";
	std::string brkon7 = "#7k";
	std::string  reply1;
	PowerPMACcontrol *ppmaccomm1 = new PowerPMACcontrol();
	
	//Reconnect to PMAC
	ppmaccomm1->PowerPMACcontrol_connect( u_ipaddr.c_str(), u_user.c_str() , u_passw.c_str(), u_port.c_str(), u_nominus2);
	sleep(1);
	
	std::string torquecom;
	torquecom = "Motor[" + std::to_string(6) + "].CompDac=" + std::to_string(0) + "\n";
	int Istorquecom = ppmaccomm1->PowerPMACcontrol_sendCommand(torquecom, reply);
	if (Istorquecom != 0)
	{
		printf("Joint [%d] torque command error.\n", 6);
	}
	else
	{
		printf("Joint [%d] torque = %f.\n", 6, 0);

	}
	//Break on all motors for safe
	ppmaccomm1->PowerPMACcontrol_sendCommand(brkon1, reply1);
	printf("Breaking on motor 1. \n");
	ppmaccomm1->PowerPMACcontrol_sendCommand(brkon2, reply1);
	printf("Breaking on motor 2. \n");

	ppmaccomm1->PowerPMACcontrol_sendCommand(brkon3, reply1);
	printf("Breaking on motor 3. \n");
	ppmaccomm1->PowerPMACcontrol_sendCommand(brkon4, reply1);
	printf("Breaking on motor 4. \n");
	ppmaccomm1->PowerPMACcontrol_sendCommand(brkon5, reply1);
	printf("Breaking on motor 5. \n");
	ppmaccomm1->PowerPMACcontrol_sendCommand(brkon6, reply1);
	printf("Breaking on motor 6. \n");
	ppmaccomm1->PowerPMACcontrol_sendCommand(brkon7, reply1);
	printf("Breaking on motor 7. \n");
	
	printf("Broke all motors. \n");
	sleep(5);
	//Disbale the selected PLC
	int isDisable = -1;
	isDisable = ppmaccomm1->PowerPMACcontrol_disablePlc(8);	//return 0 if success, minus value if has any errors
	if (isDisable != 0)
	{
	printf("Error in closing PLC. exit:\n");

	}
	else
	{
		printf("PLC is closed.\n");
	}
	
	//Turn off EtherCAT communication
	//etherCAT_Ds(ppmaccomm);
	int isDisECAT = -1;
	isDisECAT = ppmaccomm1->PowerPMACcontrol_sendCommand(ECATDs1, reply1);
	if (isDisECAT != 0)
	{
	printf("Disable etherCAT error. exit:\n");

	}
	else
	{
		printf("EtherCAT is disable.\n");
	}

	//Disconnect the PMAC
	int IsDisconnect = -1;
	IsDisconnect = ppmaccomm1->PowerPMACcontrol_disconnect();
	if (IsDisconnect != 0)
	{
	printf("Disconnect error. exit:\n");
	}
	else
	{
		printf("PMAC is disconnected.\n");

	}

	printf("PMAC and Robot are stopped correctly!\n");
	*/
	printf("Simulation is stopped!\n");
	exit(1);
}

MatrixXd JacobianComputation(double qpmac_upt[7])
{
    MatrixXd JacobComp(6,7);		//Jacobian computation matrix 6x7
    //Update Jacobian matrix
    //20190406 Results of Jacobian 6x7 - (correct)
    JacobComp(0,0) = - 1.0*d5*(sin(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + 1.0*cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) + 1.0*cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1])) - 1.0*d7*(cos(qpmac_upt[5])*(sin(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + 1.0*cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) + 1.0*cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1])) + sin(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + 1.0*cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + sin(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2])))) - d3*sin(qpmac_upt[0])*sin(qpmac_upt[1]);
    JacobComp(0,1) = 1.0*d5*(1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[3]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - 1.0*d7*(sin(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[3]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[4])) - cos(qpmac_upt[5])*(1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[3]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[3]))) + d3*cos(qpmac_upt[0])*cos(qpmac_upt[1]);
    JacobComp(0,2) = 1.0*d7*(sin(qpmac_upt[5])*(sin(qpmac_upt[4])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) - cos(qpmac_upt[3])*cos(qpmac_upt[4])*(cos(qpmac_upt[2])*sin(qpmac_upt[0]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2]))) - cos(qpmac_upt[5])*sin(qpmac_upt[3])*(cos(qpmac_upt[2])*sin(qpmac_upt[0]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2]))) - 1.0*d5*sin(qpmac_upt[3])*(cos(qpmac_upt[2])*sin(qpmac_upt[0]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2]));
    JacobComp(0,3) = - 1.0*d5*(cos(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + 1.0*cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - 1.0*d7*(cos(qpmac_upt[5])*(cos(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + 1.0*cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - cos(qpmac_upt[4])*sin(qpmac_upt[5])*(sin(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) - cos(qpmac_upt[0])*cos(qpmac_upt[3])*sin(qpmac_upt[1])));
    JacobComp(0,4) = 1.0*d7*sin(qpmac_upt[5])*(sin(qpmac_upt[4])*(cos(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - cos(qpmac_upt[4])*(cos(qpmac_upt[2])*sin(qpmac_upt[0]) + cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2])));
    JacobComp(0,5) = 1.0*d7*(sin(qpmac_upt[5])*(sin(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - cos(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + sin(qpmac_upt[4])*(cos(qpmac_upt[2])*sin(qpmac_upt[0]) + cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2]))));
    JacobComp(0,6) = 0;

    JacobComp(1,0) = d3*cos(qpmac_upt[0])*sin(qpmac_upt[1]) - d5*(sin(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) - cos(qpmac_upt[0])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - d7*(sin(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + 1.0*cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + sin(qpmac_upt[4])*(cos(qpmac_upt[2])*sin(qpmac_upt[0]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2]))) + cos(qpmac_upt[5])*(sin(qpmac_upt[3])*(sin(qpmac_upt[0])*sin(qpmac_upt[2]) - cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) - cos(qpmac_upt[0])*cos(qpmac_upt[3])*sin(qpmac_upt[1])));
    JacobComp(1,1) = d5*(cos(qpmac_upt[1])*cos(qpmac_upt[3])*sin(qpmac_upt[0]) - cos(qpmac_upt[2])*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + d7*(cos(qpmac_upt[5])*(cos(qpmac_upt[1])*cos(qpmac_upt[3])*sin(qpmac_upt[0]) - cos(qpmac_upt[2])*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - sin(qpmac_upt[5])*(cos(qpmac_upt[4])*(1.0*cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[3]) + cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1])) - 1.0*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[4]))) + d3*cos(qpmac_upt[1])*sin(qpmac_upt[0]);
    JacobComp(1,2) = d5*sin(qpmac_upt[3])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2])) - d7*(sin(qpmac_upt[5])*(sin(qpmac_upt[4])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + 1.0*cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - cos(qpmac_upt[3])*cos(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2]))) - cos(qpmac_upt[5])*sin(qpmac_upt[3])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2])));
    JacobComp(1,3) = d5*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + d7*(cos(qpmac_upt[5])*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - cos(qpmac_upt[4])*sin(qpmac_upt[5])*(sin(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) + 1.0*cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1])));
    JacobComp(1,4) = -d7*sin(qpmac_upt[5])*(sin(qpmac_upt[4])*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - 1.0*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - cos(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - 1.0*cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2])));
    JacobComp(1,5) = d7*(cos(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - 1.0*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + sin(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - 1.0*cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2]))) - sin(qpmac_upt[5])*(sin(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) + cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1])));
    JacobComp(1,6) = 0;

    JacobComp(2,0) = 0;
    JacobComp(2,1) = 1.0*d7*(sin(qpmac_upt[5])*(cos(qpmac_upt[4])*(sin(qpmac_upt[1])*sin(qpmac_upt[3]) - cos(qpmac_upt[1])*cos(qpmac_upt[2])*cos(qpmac_upt[3])) + 1.0*cos(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[4])) - 1.0*cos(qpmac_upt[5])*(cos(qpmac_upt[3])*sin(qpmac_upt[1]) + 1.0*cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[3]))) - d3*sin(qpmac_upt[1]) - d5*(cos(qpmac_upt[3])*sin(qpmac_upt[1]) + 1.0*cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[3]));
    JacobComp(2,2) = 1.0*d7*(sin(qpmac_upt[5])*(1.0*cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[4]) + cos(qpmac_upt[3])*cos(qpmac_upt[4])*sin(qpmac_upt[1])*sin(qpmac_upt[2])) + 1.0*cos(qpmac_upt[5])*sin(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[3])) + 1.0*d5*sin(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[3]);
    JacobComp(2,3) = - d5*(cos(qpmac_upt[1])*sin(qpmac_upt[3]) + 1.0*cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - 1.0*d7*(1.0*cos(qpmac_upt[5])*(cos(qpmac_upt[1])*sin(qpmac_upt[3]) + 1.0*cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) + cos(qpmac_upt[4])*sin(qpmac_upt[5])*(cos(qpmac_upt[1])*cos(qpmac_upt[3]) - cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[3])));
    JacobComp(2,4) = 1.0*d7*sin(qpmac_upt[5])*(sin(qpmac_upt[4])*(cos(qpmac_upt[1])*sin(qpmac_upt[3]) + cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) + 1.0*cos(qpmac_upt[4])*sin(qpmac_upt[1])*sin(qpmac_upt[2]));
    JacobComp(2,5) = -1.0*d7*(cos(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[1])*sin(qpmac_upt[3]) + cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - 1.0*sin(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[4])) + 1.0*sin(qpmac_upt[5])*(cos(qpmac_upt[1])*cos(qpmac_upt[3]) - 1.0*cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[3])));
    JacobComp(2,6) = 0;

    JacobComp(3,0) = -1.0*sin(qpmac_upt[0]);
    JacobComp(3,1) = cos(qpmac_upt[0])*sin(qpmac_upt[1]);
    JacobComp(3,2) = - 1.0*cos(qpmac_upt[2])*sin(qpmac_upt[0]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2]);
    JacobComp(3,3) = 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[3])*sin(qpmac_upt[1]) - 1.0*sin(qpmac_upt[3])*(1.0*sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2]));
    JacobComp(3,4) = sin(qpmac_upt[4])*(1.0*cos(qpmac_upt[3])*(1.0*sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + 1.0*cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - 1.0*cos(qpmac_upt[4])*(1.0*cos(qpmac_upt[2])*sin(qpmac_upt[0]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2]));
    JacobComp(3,5) = - 1.0*cos(qpmac_upt[5])*(1.0*sin(qpmac_upt[3])*(1.0*sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - 1.0*sin(qpmac_upt[5])*(1.0*sin(qpmac_upt[4])*(1.0*cos(qpmac_upt[2])*sin(qpmac_upt[0]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2])) + 1.0*cos(qpmac_upt[4])*(1.0*cos(qpmac_upt[3])*(1.0*sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + 1.0*cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])));
    JacobComp(3,6) = - 1.0*cos(qpmac_upt[5])*(1.0*sin(qpmac_upt[3])*(1.0*sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - 1.0*sin(qpmac_upt[5])*(1.0*sin(qpmac_upt[4])*(1.0*cos(qpmac_upt[2])*sin(qpmac_upt[0]) + 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*sin(qpmac_upt[2])) + 1.0*cos(qpmac_upt[4])*(1.0*cos(qpmac_upt[3])*(1.0*sin(qpmac_upt[0])*sin(qpmac_upt[2]) - 1.0*cos(qpmac_upt[0])*cos(qpmac_upt[1])*cos(qpmac_upt[2])) + 1.0*cos(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])));


    JacobComp(4,0) = cos(qpmac_upt[0]);
    JacobComp(4,1) = sin(qpmac_upt[0])*sin(qpmac_upt[1]);
    JacobComp(4,2) = cos(qpmac_upt[0])*cos(qpmac_upt[2]) - 1.0*cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2]);
    JacobComp(4,3) = sin(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) + 1.0*cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1]);
    JacobComp(4,4) = cos(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - 1.0*cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2])) - 1.0*sin(qpmac_upt[4])*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - 1.0*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3]));
    JacobComp(4,5) = sin(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - 1.0*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + sin(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - 1.0*cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2]))) + cos(qpmac_upt[5])*(1.0*sin(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) + 1.0*cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1]));
    JacobComp(4,6) = sin(qpmac_upt[5])*(cos(qpmac_upt[4])*(cos(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) - 1.0*sin(qpmac_upt[0])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) + sin(qpmac_upt[4])*(cos(qpmac_upt[0])*cos(qpmac_upt[2]) - 1.0*cos(qpmac_upt[1])*sin(qpmac_upt[0])*sin(qpmac_upt[2]))) + cos(qpmac_upt[5])*(1.0*sin(qpmac_upt[3])*(cos(qpmac_upt[0])*sin(qpmac_upt[2]) + cos(qpmac_upt[1])*cos(qpmac_upt[2])*sin(qpmac_upt[0])) + 1.0*cos(qpmac_upt[3])*sin(qpmac_upt[0])*sin(qpmac_upt[1]));

    JacobComp(5,0) = 0;
    JacobComp(5,1) = 1.0*cos(qpmac_upt[1]);
    JacobComp(5,2) = 1.0*sin(qpmac_upt[1])*sin(qpmac_upt[2]);
    JacobComp(5,3) = 1.0*cos(qpmac_upt[1])*cos(qpmac_upt[3]) - 1.0*cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[3]);
    JacobComp(5,4) = sin(qpmac_upt[4])*(1.0*cos(qpmac_upt[1])*sin(qpmac_upt[3]) + 1.0*cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) + 1.0*cos(qpmac_upt[4])*sin(qpmac_upt[1])*sin(qpmac_upt[2]);
    JacobComp(5,5) = cos(qpmac_upt[5])*(1.0*cos(qpmac_upt[1])*cos(qpmac_upt[3]) - 1.0*cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - 1.0*sin(qpmac_upt[5])*(1.0*cos(qpmac_upt[4])*(1.0*cos(qpmac_upt[1])*sin(qpmac_upt[3]) + 1.0*cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - 1.0*sin(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[4]));
    JacobComp(5,6) = cos(qpmac_upt[5])*(1.0*cos(qpmac_upt[1])*cos(qpmac_upt[3]) - 1.0*cos(qpmac_upt[2])*sin(qpmac_upt[1])*sin(qpmac_upt[3])) - 1.0*sin(qpmac_upt[5])*(1.0*cos(qpmac_upt[4])*(1.0*cos(qpmac_upt[1])*sin(qpmac_upt[3]) + 1.0*cos(qpmac_upt[2])*cos(qpmac_upt[3])*sin(qpmac_upt[1])) - 1.0*sin(qpmac_upt[1])*sin(qpmac_upt[2])*sin(qpmac_upt[4]));
    return JacobComp;
}

//**********************Active Stiffness contrpl from Joint 1 to 6***********************************
// 0 - joint 1; 1 - joint 2; ... ; 6- joint 7
void activestiffnesscontrol(int jstart, int jend)
{
	
    //Update current joint angles:
    currentconfig(ppmaccomm, q_pmac_update);	//in radian
    current_theta(0,0) = q_pmac_update[0];
    current_theta(1,0) = q_pmac_update[1];
    current_theta(2,0) = q_pmac_update[2];
    current_theta(3,0) = q_pmac_update[3];
    current_theta(4,0) = q_pmac_update[4];
    current_theta(5,0) = q_pmac_update[5];
    current_theta(6,0) = q_pmac_update[6];

    //Update current joint velocities current_dtheta(i,0)
    AllVelUpt = 0;
    for (int i=jstart; i<=jend; i++)
    {
        //Get from this function [deg/s] 			
        VelUpdate[i] = ppmaccomm->PowerPMACcontrol_axisGetCurrentVelocity(motor[i], current_dtheta(i,0));
        AllVelUpt = AllVelUpt + VelUpdate[i];
    }

    if (AllVelUpt != 0)
    {
        printf("Velocity updating failed. exit:\n");
    }
    else
    {
        printf("Joint angle velocities are updated.\n");
    }

    //If somethings went wrong in motor break off, velocity update -> wait the next step
    if ((AllBreakOff != 0) || (AllVelUpt != 0))
    {
        printf("Reading Vel/Breaking motors off failed. exit:\n");

    }
    //If all motors are broken off, all joint angle velocities are updated -> continue to compute torque
    else
    {
        //Converting the velocity unit from [deg/s] to [rad/s]
        for (int i=jstart; i<=jend; i++)
        {
            current_dtheta(i,0) = current_dtheta(i,0)*pi/180;
        }
        printf("Joint angle vel [rad/s] %.5f %.5f %.5f %.5f %.5f %.5f %.5f.\n", current_dtheta(0,0), current_dtheta(1,0), current_dtheta(2,0), current_dtheta(3,0), current_dtheta(4,0), current_dtheta(5,0), current_dtheta(6,0));

        
        //Compute joint position error [rad]
        for (int i=jstart; i<=jend; i++)
        {
            theta_err1(i,0) = desired_theta(i,0) - current_theta(i,0);
        }
        printf("Joint angle errors: %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n", theta_err1(0,0), theta_err1(1,0), theta_err1(2,0), theta_err1(3,0), theta_err1(4,0), theta_err1(5,0), theta_err1(6,0));

        //Joint velocity error convert  for PD control
        for (int i=jstart; i<=jend; i++)
        {
            dtheta_err(i,0) = (theta_err1(i,0) - theta_err2(i,0))/time_taken;
            //Back up for next loop
            theta_err2(i,0) = theta_err1(i,0);
        }
        

        //Friction and Gravity compensate based on actual positions and velocities
        for (int i=jstart; i<=jend; i++)
        {
            //Friction compensate Torque function -> Ampere (A)			
            //If Vel << --- set 0: filter
            if (fabs(current_dtheta(i,0)) < dtheta_threshold) current_dtheta(i,0) = 0;
        
            //Friction compensator function
            Friction_I(i,0) = frictioncomptorque(i, current_dtheta(i,0));

            //If >> positive --- set fric_amp_lim[i] rad/s
            if (current_dtheta(i,0) > fric_amp_lim[i]) Friction_I(i,0) = fric_amp_lim[i];
            //If >> negative --- set -fric_amp_lim[i] rad/s
            else if (current_dtheta(i,0) < -fric_amp_lim[i]) Friction_I(i,0) = -fric_amp_lim[i];

            //Gravity compensate torque -> [Nm]
            Gravity_T(i,0) = gravitycomptorque(i, q_pmac_update);
        }
        
        //Update Jacobian matrix
        Jacob = JacobianComputation(q_pmac_update);
         
        //Compute Jacobian transpose and pseudo inverse
        Trans_Jacob = Jacob.transpose();

        //Joint stiffness matrix 7x7
        Kmin = Trans_Jacob*Kx*Jacob;
        Knull = (I7-K0*Trans_Jacob*(Jacob*K0*Trans_Jacob).inverse()*Jacob)*K0;

        Ki = Kmin + Knull;

        //Joint damping matrix 7x7
        Di = Trans_Jacob*Dx*Jacob;
    
        //std::cout << Ki << sep;
        //std::cout << Di << sep;
    
        //Torque computed from stiffness control
        //Di(1,1) = Di(1,1)*1.2;
        Tau_stif = Ki*theta_err1 + Di*dtheta_err;
        //Adding friction and gravity terms, convert to current [Ampere]
        int TorqIsSet[7], AllTorqueSet;
        AllTorqueSet = 0;
        for (int i=jstart; i<=jend; i++)
        {
            //Command torque
            Tau_I(i,0) = Tau_stif(i,0) + Gravity_T(i,0) + Friction_I(i,0)*Kt[i];
            
            //Tau_I(i,0) = ((Tau_stif(i,0) + Gravity_T(i,0))*1000)/(Nhd[i]*Kt[i]*Icon[i]) + Friction_I(i,0)*1000/Icon[i];	//joint i
            //Tau_I(i,0) = (Gravity_T(i,0)*1000)/(Nhd[i]*Kt[i]*Icon[i]) + Friction_I(i,0)*1000/Icon[i];	//joint i
            //Joint torque limits
            //if (i==1) Tau_I(i,0) = 1.15*Tau_I(i,0);
            //if (i==3) Tau_I(i,0) = 1.3*Tau_I(i,0);
            //if (i==5) Tau_I(i,0) = 1.3*Tau_I(i,0);
            if (Tau_I(i,0) > T_limit[i])
            {
                Tau_I(i,0) = (T_limit[i]*1000)/(160*Kt[i]*Icon[i]); //1000 * 0.1% = 100%
                if(Tau_I(i,0) > 300) {Tau_I(i,0) = 300;}	//30% 
            }
            else if (Tau_I(i,0) < -T_limit[i])
            {
                Tau_I(i,0) = -(T_limit[i]*1000)/(160*Kt[i]*Icon[i]); //1000 * 0.1% = 100%
                if(Tau_I(i,0) < -300) {Tau_I(i,0) = -300;}	//30% 
            }
            else
            {
                Tau_I(i,0)=(Tau_I(i,0)*1000)/(160*Kt[i]*Icon[i]);
				if(Tau_I(i,0) > 300) {Tau_I(i,0) = 300;}
				if(Tau_I(i,0) < -300) {Tau_I(i,0) = -300;}
            }
            
            //Send to PMAC
            TorqIsSet[i] = ppmaccomm->PowerPMACcontrol_axisSetTorqueCommand(motor[i], Tau_I(i,0));		
            AllTorqueSet = AllTorqueSet + TorqIsSet[i];
            printf("Joint [%d]:T_PD [%.2f] T_FR [%.2f] T_GR [%.2f] \n", i+1, Tau_stif(i,0), Friction_I(i,0)*Kt[i], Gravity_T(i,0));
        }
        //Publish to Gazebo		
		joint_publish(q_pmac_update);

        if (AllTorqueSet != 0)
        {
            printf("Torque sending failed. exit:\n");	

        }
        else
        {
            printf("All Torque are sent: [%.2f] [%.2f] [%.2f] [%.2f] [%.2f] [%.2f] [%.2f] \n", Tau_I(0,0), Tau_I(1,0), Tau_I(2,0), Tau_I(3,0), Tau_I(4,0), Tau_I(5,0), Tau_I(6,0));
        }
    }
			
}

//****************************************MAIN PROGRAM************************************************
int main(int argc, char **argv)

{
	std::string sep = "\n----------------------------------------\n";
	ros::init(argc, argv, "torque_downscale");  // Node name initialization
	ros::NodeHandle nh;                            // Node handle declaration for communication with ROS

	pub_jointp = nh.advertise<downscale4::seven>("gazebo/downscale_jointp", 100);
	pub_jointt = nh.advertise<downscale4::seven>("gazebo/downscale_jointt", 100);

    ros::Subscriber hd_trans = nh.subscribe("/hd_trans", 1, &hd_callback_trans);        //sub transformation matrix
	ros::Subscriber hd_buttons = nh.subscribe("/hd_buttons", 1, &hd_callback_buttons);  //sub phantom omni buttons
	ros::Subscriber hd_char = nh.subscribe("/hd_char", 1, &hd_callback_keyboards); 
    ros::Subscriber sub_jointp = nh.subscribe("downscale_actp", 100, msgCallbackP);     //sub joint position
	ros::Subscriber sub_jointt = nh.subscribe("downscale_actt", 100, msgCallbackT);     //sub joint torque
	ros::Subscriber sub_jointv = nh.subscribe("downscale_actv", 100, msgCallbackv);     //sub joint velocity
    

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
	//***********************************Local constants******************************************
	//Link masses 
	//real masses
	/*
	m[0]=14;		//Link 1 mass
	m[1]=30;		//Link 2 mass
	m[2]=25;		//Link 3 mass
	m[3]=15;		//Link 4 mass
	m[4]=6.3;		//Link 5 mass
	m[5]=6.74;		//Link 6 mass
	m[6]=4.6;		//Link 7 mass + End Effector
	*/
	//gazebo sdf masses
	m[0]=3.8202;		//Link 1 mass
	m[1]=2.9008;		//Link 2 mass
	m[2]=2.0424;		//Link 3 mass
	m[3]=1.7764;		//Link 4 mass
	m[4]=1.1622;		//Link 5 mass
	m[5]=1.1200;		//Link 6 mass
	m[6]=0.1554;		//Link 7 mass + End Effector

	//motor index
	motor[0]=1;		//motor 1 index
	motor[1]=2;		//motor 2 index
	motor[2]=3;		//motor 3 index
	motor[3]=4;		//motor 4 index
	motor[4]=5;		//motor 5 index
	motor[5]=6;		//motor 6 index
	motor[6]=7;		//motor 7 index

	//motor torque constants
	Kt[0]=0.3656;	//motor 1 torque constant (1/A)
	Kt[1]=0.4401;	//motor 2 torque constant (1/A)
	Kt[2]=0.4626;	//motor 3 torque constant (1/A)
	Kt[3]=0.4626;	//motor 4 torque constant (1/A)
	Kt[4]=0.3658;	//motor 5 torque constant (1/A)
	Kt[5]=0.3658;	//motor 6 torque constant (1/A)
	Kt[6]=0.2325;	//motor 7 torque constant (1/A)

	//Harmonic driver gear ratio
	Nhd[0]=160;	//Harmonic driver 1 gear ratio
	Nhd[1]=160;	//Harmonic driver 2 gear ratio
	Nhd[2]=160;	//Harmonic driver 3 gear ratio
	Nhd[3]=160;	//Harmonic driver 4 gear ratio
	Nhd[4]=160;	//Harmonic driver 5 gear ratio
	Nhd[5]=160;	//Harmonic driver 6 gear ratio
	Nhd[6]=120;	//Harmonic driver 7 gear ratio

	//motor driver continuos current
	Icon[0]=2.744;	//motor 1 driver continuous current (A)
	Icon[1]=8.033;	//motor 2 driver continuous current (A)
	Icon[2]=4.101;	//motor 3 driver continuous current (A)
	Icon[3]=4.101;	//motor 4 driver continuous current (A)
	Icon[4]=2.744;	//motor 5 driver continuous current (A)
	Icon[5]=2.63;	//motor 6 driver continuous current (A)
	Icon[6]=1.131;	//motor 7 driver continuous current (A)

	//PD control gains
	Kp[0]=120;	//P gain for joint 1
	Kp[1]=300;	//P gain for joint 2
	Kp[2]=100;	//P gain for joint 3
	Kp[3]=180;	//P gain for joint 4
	Kp[4]=90;	//P gain for joint 5
	Kp[5]=55;	//P gain for joint 6
	Kp[6]=50;	//P gain for joint 7

	Kd[0]=3;	//D gain for joint 1
	Kd[1]=14;	//D gain for joint 2
	Kd[2]=3;	//D gain for joint 3
	Kd[3]=6;	//D gain for joint 4
	Kd[4]=3;	//D gain for joint 5
	Kd[5]=0.3;	//D gain for joint 6
	Kd[6]=0.4;	//D gain for joint 7


	//Torque limits
	T_limit[0] = 200;
	T_limit[1] = 500;
	T_limit[2] = 350;
	T_limit[3] = 350;
	T_limit[4] = 200;
	T_limit[5] = 300;
	T_limit[6] = 300;

	//Friction compensate current (A) limit +/-
	fric_amp_lim[0] = 0.6;	//Joint 1
	fric_amp_lim[1] = 1.4;	//Joint 2
	fric_amp_lim[2] = 0.95;	//Joint 3
	fric_amp_lim[3] = 0.65;	//Joint 4
	fric_amp_lim[4] = 0.65;	//Joint 5
	fric_amp_lim[5] = 0.5;	//Joint 6
	fric_amp_lim[6] = 0.3;	//Joint 7

    //Init transformation matrix
    P_INIT << 	1, 	 0, 	 0, 	0.4,
                0, 	 -1, 	 0, 	0.4,
                0, 	 0, 	 -1, 	0.4,
                0, 	 0, 	 0, 	  1;
	P_END = P_INIT;
	p_e(0,0) = P_INIT(0,3);	//x
	p_e(1,0) = P_INIT(1,3);	//y
	p_e(2,0) = P_INIT(2,3);	//z
    //Init position
	robot_initial_xyz[0] = P_INIT(0,3);
	robot_initial_xyz[1] = P_INIT(1,3);
	robot_initial_xyz[2] = P_INIT(2,3);

    robot_initial_rotation <<		P_INIT(0,0), 	P_INIT(0,1), 	P_INIT(0,2), 	
        							P_INIT(1,0), 	P_INIT(1,1), 	P_INIT(1,2), 	
                					P_INIT(2,0), 	P_INIT(2,1), 	P_INIT(2,2);

	robot_current_rotation = robot_initial_rotation;
               
	hd_del_rotation <<			1, 0, 0,
								0, 1, 0,
								0, 0, 1;

	//***********************************Hammering task variables******************************************
	//Given EE homogeneous transformation matrix 
	P_HAM << 	1, 	0, 	0, 	-0.4,
				0, 	-1, 0, 	0.0,
				0, 	0, 	-1, 0.1,
				0, 	0, 	0, 	1;

    //Initial Joint configuration
	InvK7(P_INIT, SI_del);
	desired_theta(0,0) = q[0];		//Joint 1 desired angle	
	desired_theta(1,0) = q[1];		//Joint 2 desired angle
	desired_theta(2,0) = q[2];		//Joint 3 desired angle
	desired_theta(3,0) = q[3];	//Joint 4 desired angle
	desired_theta(4,0) = q[4];	//Joint 5 desired angle
	desired_theta(5,0) = q[5];		//Joint 6 desired angle
	desired_theta(6,0) = q[6];		//Joint 7 desired angle

	//init joint error:
	theta_err1(0,0) = 0;
	theta_err1(1,0) = 0;
	theta_err1(2,0) = 0;	
	theta_err1(3,0) = 0;
	theta_err1(4,0) = 0;
	theta_err1(5,0) = 0;
	theta_err1(6,0) = 0;	

	theta_err2(0,0) = 0;
	theta_err2(1,0) = 0;
	theta_err2(2,0) = 0;	
	theta_err2(3,0) = 0;
	theta_err2(4,0) = 0;
	theta_err2(5,0) = 0;
	theta_err2(6,0) = 0;


	Ki(0,0) = 100;
	Ki(1,0) = 100;
	Ki(2,0) = 100;
	Ki(3,0) = 100;
	Ki(4,0) = 100;
	Ki(5,0) = 100;
	Ki(6,0) = 100;

	Di(0,0) = 20;
	Di(1,0) = 20;
	Di(2,0) = 20;
	Di(3,0) = 20;
	Di(4,0) = 20;
	Di(5,0) = 20;
	Di(6,0) = 20;	

	//****************************Stiffness control parameters************************************
	//Cartesian stiffness matrix 6x6 [N/m] - Original
	/*
	KxVec << 2500, 500, 2500, 1, 1, 1;
	Kx = KxVec.asDiagonal();

	//Cartesian damping matrix 6x6
	DxVec << 250, 50, 250, 1, 1, 1;
	Dx = DxVec.asDiagonal();

	//Weighting matrix 7x7
	K0Vec << 10, 10, 10, 10, 10, 10, 10;
	K0 = K0Vec.asDiagonal();

	I7 = I7.setIdentity(7,7);
	*/

	//********************create an SSH connection to Power PMAC**********************************
	int estatus = ppmaccomm->PowerPMACcontrol_connect( u_ipaddr.c_str(), u_user.c_str() , u_passw.c_str(), u_port.c_str(), u_nominus2);	
	if (estatus != 0)
	{
	printf("Error connecting to power pmac. exit:\n");
	}
	else
	{
		printf("Connected OK.\n");
	}
	sleep(1);
	
	
	//***********************Initialize the PMAC and Manipulator**********************************
	
	disablePLC(ppmaccomm, selected_PLC);
	sleep(1);
	
	enablePLC(ppmaccomm, selected_PLC);
	sleep(1);
	
	ppmaccomm->PowerPMACcontrol_sendCommand(ECATDs, reply);
	sleep(1);
	
	int CommandIsSend = ppmaccomm->PowerPMACcontrol_sendCommand(ECATEn, reply);
	if (CommandIsSend != 0)
	{
		printf("Enable EtherCAT failed. exit:\n");
	
	}
	else
	{
		printf("EtherCAT is enable.\n");

	}
	sleep(20);

	currentconfig(ppmaccomm, q_pmac_update);	//good, radian

	
	AllBreakOff = 0;
	for (int i=3; i<4; i++)
	{
		MotorBreakOff[i] = ppmaccomm->PowerPMACcontrol_axisBreakOffTorque(motor[i]);
		AllBreakOff = AllBreakOff + MotorBreakOff[i];
		delay_ms(1200);
	}

	if (AllBreakOff != 0)
	{
		printf("Joint breaking off failed. exit:\n");
	}
	else
	{
		printf("Break off all joints in torque mode.\n");
	}
	
	
	sleep(1);
    joint_publish(q_pmac_update);
	ros::spinOnce();

    int jointstart = 0;
    int jointend = 6;
    time_taken = 0.003;
	int VelIsGet[7];
	psi_ham = 0.6;	
	bool psi_adj = 0;
	float rate = 10000;
	ros::Rate loop_rate(rate);
	//***************************Stiffness control infinite loop**********************************
	while (ros::ok())
	{  	
		inz = 0.3;
		//Cartesian stiffness matrix 6x6 [N/m]
		KxVec << 2500, 2500, 2500, 1000, 1000, 1000;
		Kx = KxVec.asDiagonal();

		//Cartesian damping matrix 6x6
		DxVec << 8, 8, 8, 5, 5, 5;
		Dx = DxVec.asDiagonal();

		//Weighting matrix 7x7
		K0Vec << 3000, 3000, 3000, 3000, 3000, 3000, 3000;
		K0 = K0Vec.asDiagonal();

		if (ReadKey == '+')
		{
			//Avoiding arm configuration change  -> re-compute all q[i] after change J1
			//q[0] = desired_theta(0,0);
			q[1] = desired_theta(1,0);
			q[2] = desired_theta(2,0);
			q[3] = desired_theta(3,0);
			q[4] = desired_theta(4,0);
			q[5] = desired_theta(5,0);
			q[6] = desired_theta(6,0);

			desired_theta(0,0) = desired_theta(0,0) + 0.0025;
			q[0] = desired_theta(0,0);
			
			P_INIT = forwd7(q);

			robot_initial_xyz[0] = P_INIT(0,3);
			robot_initial_xyz[1] = P_INIT(1,3);
			robot_initial_xyz[2] = P_INIT(2,3);

			P_INIT << 	1, 	 0, 	 0, 	robot_initial_xyz[0],
                		0, 	-1, 	 0, 	robot_initial_xyz[1],
                		0, 	 0, 	-1, 	robot_initial_xyz[2],
                		0, 	 0, 	 0, 	  1;


			InvK7(P_INIT, SI_del);
			desired_theta(0,0) = q[0];		//Joint 1 desired angle	
			desired_theta(1,0) = q[1];		//Joint 2 desired angle
			desired_theta(2,0) = q[2];		//Joint 3 desired angle
			desired_theta(3,0) = q[3];	//Joint 4 desired angle
			desired_theta(4,0) = q[4];	//Joint 5 desired angle
			desired_theta(5,0) = q[5];		//Joint 6 desired angle
			desired_theta(6,0) = q[6];		//Joint 7 desired angle
			
			hd_del_xyz[0] = 0;          //Position changing is 0
			hd_del_xyz[1] = 0;
			hd_del_xyz[2] = 0;
						
			haptic_del_xyz[0] = 0;
			haptic_del_xyz[1] = 0;
			haptic_del_xyz[2] = 0;
						
			hd_del_rotation <<			1, 0, 0,
										0, 1, 0,
										0, 0, 1;

			P_END = P_INIT;
			//Save position value for hammering task if have
			p_e(0,0) = P_END(0,3);	//x
			p_e(1,0) = P_END(1,3);	//y
			p_e(2,0) = P_END(2,3);	//z

			ReadKey = '0';
		}
		if (ReadKey == '-')
		{
			//Avoiding arm configuration change  -> re-compute all q[i] after change J1
			//q[0] = desired_theta(0,0);
			q[1] = desired_theta(1,0);
			q[2] = desired_theta(2,0);
			q[3] = desired_theta(3,0);
			q[4] = desired_theta(4,0);
			q[5] = desired_theta(5,0);
			q[6] = desired_theta(6,0);

			desired_theta(0,0) = desired_theta(0,0) - 0.0025;
			q[0] = desired_theta(0,0);
			
			P_INIT = forwd7(q);

			robot_initial_xyz[0] = P_INIT(0,3);
			robot_initial_xyz[1] = P_INIT(1,3);
			robot_initial_xyz[2] = P_INIT(2,3);
			
			P_INIT << 	1, 	 0, 	 0, 	robot_initial_xyz[0],
                		0, 	-1, 	 0, 	robot_initial_xyz[1],
                		0, 	 0, 	-1, 	robot_initial_xyz[2],
                		0, 	 0, 	 0, 	  1;


			InvK7(P_INIT, SI_del);
			desired_theta(0,0) = q[0];		//Joint 1 desired angle	
			desired_theta(1,0) = q[1];		//Joint 2 desired angle
			desired_theta(2,0) = q[2];		//Joint 3 desired angle
			desired_theta(3,0) = q[3];	//Joint 4 desired angle
			desired_theta(4,0) = q[4];	//Joint 5 desired angle
			desired_theta(5,0) = q[5];		//Joint 6 desired angle
			desired_theta(6,0) = q[6];		//Joint 7 desired angle

			
			hd_del_xyz[0] = 0;          //Position changing is 0
			hd_del_xyz[1] = 0;
			hd_del_xyz[2] = 0;
						
			haptic_del_xyz[0] = 0;
			haptic_del_xyz[1] = 0;
			haptic_del_xyz[2] = 0;

			hd_del_rotation <<			1, 0, 0,
										0, 1, 0,
										0, 0, 1;

			P_END = P_INIT;
			//Save position value for hammering task if have
			p_e(0,0) = P_END(0,3);	//x
			p_e(1,0) = P_END(1,3);	//y
			p_e(2,0) = P_END(2,3);	//z

			ReadKey = '0';
		}

        //********************************************************************
		//Insert Inkwell & click-hold button 2 for entering psi adjusting mode
		if (Inkwell == 0)
		{
			if (Buttons[1] == 1)
			{
				button1_click_time++;
				if (button1_click_time == 200) 
				{
					psi_adj = 1;
					button1_click_time = 0;
				}
			}
			if (psi_adj == 1)
			{
				if (Buttons[0] == 1)
				//Increasing psi
				{
					SI_del = SI_del + 0.00005;
					InvK7(P_END, SI_del);
					desired_theta(0,0) = q[0];		//Joint 1 desired angle	
					desired_theta(1,0) = q[1];		//Joint 2 desired angle
					desired_theta(2,0) = q[2];		//Joint 3 desired angle
					desired_theta(3,0) = q[3];		//Joint 4 desired angle
					desired_theta(4,0) = q[4];		//Joint 5 desired angle
					desired_theta(5,0) = q[5];		//Joint 6 desired angle
					desired_theta(6,0) = q[6];		//Joint 7 desired angle
					//Save position value for hammering task if have
					p_e(0,0) = P_END(0,3);	//x
					p_e(1,0) = P_END(1,3);	//y
					p_e(2,0) = P_END(2,3);	//z
					button1_click_time = 0;
				}
				if (Buttons[1] == 1)
				//Increasing psi
				{
					SI_del = SI_del - 0.00005;
					InvK7(P_END, SI_del);
					desired_theta(0,0) = q[0];		//Joint 1 desired angle	
					desired_theta(1,0) = q[1];		//Joint 2 desired angle
					desired_theta(2,0) = q[2];		//Joint 3 desired angle
					desired_theta(3,0) = q[3];		//Joint 4 desired angle
					desired_theta(4,0) = q[4];		//Joint 5 desired angle
					desired_theta(5,0) = q[5];		//Joint 6 desired angle
					desired_theta(6,0) = q[6];		//Joint 7 desired angle
					//Save position value for hammering task if have
					p_e(0,0) = P_END(0,3);	//x
					p_e(1,0) = P_END(1,3);	//y
					p_e(2,0) = P_END(2,3);	//z
					button1_click_time = 0;
				
				}
			}
		}

        //********************************************************************
		//Release Inkwell & click-hold button 2 for exit psi adjusting mode
		if ((Inkwell == 1) && (psi_adj == 1))
		{
			if (Buttons[1] == 1)
			{
				button1_click_time++;
				if (button1_click_time == 100) 
				{
					psi_adj = 0;
					button1_click_time = 0;
				}
			}
		}

        //********************************************************************
		//Release Inkwell & click-drag button 1 to adjusting arm configuration
        if ((Buttons[0] == 1) && (Inkwell == 1) && (psi_adj == 0))
        //EE Phantom omni gets out init pos, button0 is pushed  
        {
            
			if (button0_click_time < 200)
            //If buttons0 is clicked first time -> set this current pos, rot are init pos, rot
			{
				hd_initial_xyz[0] = hd_current_xyz[0];
				hd_initial_xyz[1] = hd_current_xyz[1];
				hd_initial_xyz[2] = hd_current_xyz[2];
			
				hd_initial_rotation <<		hd_rotation_matrix[0], 	hd_rotation_matrix[3], 	hd_rotation_matrix[6], 	
        									hd_rotation_matrix[1], 	hd_rotation_matrix[4], 	hd_rotation_matrix[7], 	
                							hd_rotation_matrix[2], 	hd_rotation_matrix[5], 	hd_rotation_matrix[8]; 			
			}
            //If button0 click-time is not 1 -> compute position and rotation changing delta 
            hd_del_xyz[0] = hd_current_xyz[0] - hd_initial_xyz[0];
			hd_del_xyz[1] = hd_current_xyz[1] - hd_initial_xyz[1];
			hd_del_xyz[2] = hd_current_xyz[2] - hd_initial_xyz[2];
			haptic_del_xyz[0] = haptic_del_xyz[0] - hd_del_xyz[2];
			haptic_del_xyz[1] = haptic_del_xyz[1] - hd_del_xyz[0];
			haptic_del_xyz[2] = haptic_del_xyz[2] + hd_del_xyz[1];
			hd_initial_xyz[0] = hd_current_xyz[0];
			hd_initial_xyz[1] = hd_current_xyz[1];
			hd_initial_xyz[2] = hd_current_xyz[2];

			hd_del_rotation = hd_current_rotation*hd_initial_rotation.transpose();
			//haptic_del_rotation = oil*hd_del_rotation;
			//rotation matrix to Euler angle

			Euler_b=(-asin(hd_del_rotation(2,0)))/1;
			if (Euler_b != 90*pi/180 && Euler_b != -90*pi/180)
			{
				Euler_a=(atan2(hd_del_rotation(1,0)/cos(Euler_b),hd_del_rotation(0,0)/cos(Euler_b)))/1;
				Euler_r=(atan2(hd_del_rotation(2,1)/cos(Euler_b),hd_del_rotation(2,2)/cos(Euler_b)))/1;
			}
			else if (Euler_b == 90*pi/180)
			{
				Euler_b=(90*pi/180)/1;
				Euler_a=0/1;
				Euler_r=(atan2(hd_del_rotation(0,1),hd_del_rotation(1,1)))/1;
			}
			else
			{
				Euler_b=(-90*pi/180)/1;
				Euler_a=0/1;
				Euler_r=(-atan2(hd_del_rotation(0,1),hd_del_rotation(1,1)))/1;
			}

			Euler_x << 	1, 			   0, 				0, 	
                    	0, 	cos(Euler_r), 	-sin(Euler_r), 	
                    	0, 	sin(Euler_r), 	cos(Euler_r);
						
			Euler_y << 	cos(Euler_b),  0, 	 sin(Euler_b), 	
                    	0, 			   1,			 	0, 	
                    	-sin(Euler_b), 0, 	 cos(Euler_b);			 	

			Euler_z << 	cos(Euler_a), -sin(Euler_a),	0, 	
                    	sin(Euler_a),  cos(Euler_a), 	0, 	
                    	0, 						  0, 	1;

			Euler_angle=Euler_z*Euler_y*Euler_x;
						
			robot_current_rotation = Euler_angle*robot_current_rotation;
			hd_initial_rotation = hd_current_rotation;

            //Update the new desired pose
            P_END << 	1, 	0, 	0, 	robot_initial_xyz[0] + haptic_del_xyz[0]*0.0015,
						0, -1, 	0, 	robot_initial_xyz[1] + haptic_del_xyz[1]*0.0015,
						0, 	0, -1, 	robot_initial_xyz[2] + haptic_del_xyz[2]*0.0015,
						0, 	0, 	0, 	1;
		

			//Save position value for hammering task if have
			p_e(0,0) = P_END(0,3);	//x
			p_e(1,0) = P_END(1,3);	//y
			p_e(2,0) = P_END(2,3);	//z
            
			button0_click_time++;
            //printf("button clicked %d \n",button0_click_time);
            //From current desired pose -> control using stiffness control
			InvK7(P_END, SI_del);
			desired_theta(0,0) = q[0];		//Joint 1 desired angle	
			desired_theta(1,0) = q[1];		//Joint 2 desired angle
			desired_theta(2,0) = q[2];		//Joint 3 desired angle
			desired_theta(3,0) = q[3];		//Joint 4 desired angle
			desired_theta(4,0) = q[4];		//Joint 5 desired angle
			desired_theta(5,0) = q[5];		//Joint 6 desired angle
			desired_theta(6,0) = q[6];		//Joint 7 desired angle
        }
        else
        {
            button0_click_time = 0;     //reset button0 click counting
			hd_del_xyz[0] = 0;          //Position changing is 0
			hd_del_xyz[1] = 0;
			hd_del_xyz[2] = 0;
			
			hd_del_rotation <<		1, 	0, 	0, 	
        							0, 	1, 	0, 	
                					0, 	0, 	1;
        }

        

        //STIFFNESS CONTROL FROM HERE
        clock_t t; 
        t = clock();          
        activestiffnesscontrol(jointstart,jointend);
		loop_rate.sleep();
        ros::spinOnce();
		t = clock() - t; 
		time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
        //Check for hammering requirement?
        //Hammering TASK if have
		//********************************************************************
		//Insert Inkwell & click button 1 to do hammering task
        if (Buttons[0] == 1 && Inkwell == 0 && psi_adj == 0)
        //If Inkwell is at init pos, button0 is clicked -> Do the HAMMERING task at this pos
        {
            //Adjusting cartesian stiffness along z direction for hammering task
			//Cartesian stiffness matrix 6x6 [N/m]
			KxVec << 2500, 2500, 400, 1000, 1000, 1000;
			Kx = KxVec.asDiagonal();

			//Cartesian damping matrix 6x6
			DxVec << 8, 8, 8, 0.6, 5, 5;
			Dx = DxVec.asDiagonal();

			//Weighting matrix 7x7
			K0Vec << 3000, 3000, 3000, 3000, 3000, 3000, 3000;
			K0 = K0Vec.asDiagonal();
			
			p_save(0,0) = p_e(0,0);	//x
			p_save(1,0) = p_e(1,0);	//y
			p_save(2,0) = p_e(2,0);	//z

			//MOVE UP ***************************************************************
			for(inz = 0; inz <= 0.2; inz = inz + 0.000125)
			{
				
				P_HAM << 	1, 	0, 	0, 	p_save(0,0),
							0, -1, 	0, 	p_save(1,0),
							0, 	0, -1, 	p_save(2,0) + inz,
							0, 	0, 	0, 	1;

				InvK7(P_HAM, SI_del);
				desired_theta(0,0) = q[0];		//Joint 1 desired angle	
				desired_theta(1,0) = q[1];		//Joint 2 desired angle
				desired_theta(2,0) = q[2];		//Joint 3 desired angle
				desired_theta(3,0) = q[3];		//Joint 4 desired angle
				desired_theta(4,0) = q[4];		//Joint 5 desired angle
				desired_theta(5,0) = q[5];		//Joint 6 desired angle
				desired_theta(6,0) = q[6];		//Joint 7 desired angle

				clock_t t; 
				t = clock();
				activestiffnesscontrol(jointstart,jointend);
				loop_rate.sleep();
				ros::spinOnce();
				t = clock() - t; 
				time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
			}
			

			//MOVE DOWN *************************************************************
			// 0.2/0.0005 = 400 steps; 0.000625 = 320 steps
			for(inz = 0.2; inz >= 0; inz = inz - 0.0005)
			{
				P_HAM << 	1, 	0, 	0, 	p_save(0,0),
							0, -1, 	0, 	p_save(1,0),
							0, 	0, -1, 	p_save(2,0) + inz,
							0, 	0, 	0, 	1;

				InvK7(P_HAM, SI_del);
				desired_theta(0,0) = q[0];		//Joint 1 desired angle	
				desired_theta(1,0) = q[1];		//Joint 2 desired angle
				desired_theta(2,0) = q[2];		//Joint 3 desired angle
				desired_theta(3,0) = q[3];		//Joint 4 desired angle
				desired_theta(4,0) = q[4];		//Joint 5 desired angle
				desired_theta(5,0) = q[5];		//Joint 6 desired angle
				desired_theta(6,0) = q[6];		//Joint 7 desired angle

				clock_t t; 
				t = clock();
				activestiffnesscontrol(jointstart,jointend);
				loop_rate.sleep();
				ros::spinOnce();
				t = clock() - t; 
				time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
			}
			
            //Update the new desired pose
			//Compute phi angle
			P_END << 	1, 	 0, 	 0, 	p_save(0,0),
						0, 	 -1, 	 0, 	p_save(1,0),
						0, 	 0, 	 -1, 	p_save(2,0),
						0, 	 0, 	 0, 	  1;
			//Save for the next hammering task
			p_e(0,0) = P_END(0,3);	//x
			p_e(1,0) = P_END(1,3);	//y
			p_e(2,0) = P_END(2,3);	//z
			//From current desired pose -> control using stiffness control
			InvK7(P_END, SI_del);
			desired_theta(0,0) = q[0];		//Joint 1 desired angle	
			desired_theta(1,0) = q[1];		//Joint 2 desired angle
			desired_theta(2,0) = q[2];		//Joint 3 desired angle
			desired_theta(3,0) = q[3];		//Joint 4 desired angle
			desired_theta(4,0) = q[4];		//Joint 5 desired angle
			desired_theta(5,0) = q[5];		//Joint 6 desired angle
			desired_theta(6,0) = q[6];		//Joint 7 desired angle
        } //end if if (Buttons[0] == 1 && Inkwell == 1)

  
    } //end while main
	//myfile.close();
	return 0;	
} //end main

