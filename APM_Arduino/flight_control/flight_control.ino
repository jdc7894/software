#include <AP_ADC_HIL.h>
#include <AP_ADC.h>

#include <AP_Common.h>
#include <AP_Math.h>
#include <AP_Param.h>
#include <AP_Progmem.h>

#include <AP_HAL.h>
#include <AP_HAL_AVR.h>

#include <AP_InertialSensor.h>
#include <GCS_MAVLink.h>

#include <RC_Channel.h>     // RC Channel Library
#include <AP_Motors.h>
#include <AP_Curve.h>

#include <Filter.h>
#include <LowPassFilter2p.h>

#include <AP_Notify.h>

#include <PID.h> //pid controller
#include <AC_PID.h>

#include "flight_control.h"

Flight_Control::Flight_Control() : rPid(R_P, R_I, R_D, R_IMAX), 
                                   pPid(P_P, P_I, P_D, P_IMAX),
                                   tPid(T_P, T_I, T_D, T_IMAX),
                                   yPid(Y_P, Y_I, Y_D, Y_IMAX),
                                   acc_offset(ACC_OFFSET_X, ACC_OFFSET_Y, ACC_OFFSET_Z) {
    m_roll = new RC_Channel(2);
    m_pitch = new RC_Channel(3);
    m_throttle = new RC_Channel(1);
    m_yaw = new RC_Channel(4);
    motors = new AP_MotorsQuad(m_roll, m_pitch, m_throttle, m_yaw);
    setGyrFactor(150);
    
    arm(false);
    ins.init(AP_InertialSensor::COLD_START, AP_InertialSensor::RATE_100HZ);

// HAL will start serial port at 115200.
    hal.console->println_P(PSTR("Starting!"));

//defines the mapping system from RC command to motor effect (defined in quad)
    motors->set_frame_orientation(AP_MOTORS_X_FRAME);//motors.set_frame_orientation(AP_MOTORS_H_FRAME);
//motors.min_throttle is in terms of servo values not output values.

//setup rc
    //setup_m_rc();
    //setup PID Control
    setRollPID(rPid);
    setPitchPID(pPid);
    setThrottlePID(tPid);
    setYawPID(yPid);

    //setup motors
    motors->Init();
    motors->set_update_rate(500);
    motors->enable();

    //setup timing
    timestamp = hal.scheduler->micros();

    //kill the barometer
    hal.gpio->pinMode(40, GPIO_OUTPUT);
    hal.gpio->write(40,1);
}

void Flight_Control::arm(bool armed){
    this->armed = armed;
    motors->armed(armed);
}

/*void Flight_Control::setup_m_rc(){
  m_throttle.set_range(0,1000);//should be 1000 probs.
    m_roll.set_range    (0,1000);
    m_pitch.set_range   (0,1000);
    m_yaw.set_range     (0,1000);

    m_throttle.radio_min = 1000;
    m_throttle.radio_max = 2200;

    m_roll.servo_out = 0;
    m_pitch.servo_out = 0;
    m_yaw.servo_out = 0;
}
*/

//Get & Set for Gyroscope Error Scale
void Flight_Control::setGyrFactor(float f){
  this->gyrErrScale = f;
}
float Flight_Control::getGyrFactor(){
  return this->gyrErrScale;
}

//Execute a single command, given an up vector, throttle value, and current yaw
void Flight_Control::execute(Vector3f& cntrl_up, float cntrl_throttle, float cntrl_yaw){
    if(!armed) return;

    cntrl_up = cntrl_up.normalized();

    //update the time locks/ time updates
    int t = hal.scheduler->micros();
    float dt = (t - timestamp)/1000.0;
    timestamp = t;


    //get new instrument measurement
    ins.update();
    
    //compute the control value corresponding to current IMU output
    //should be scaled to -100 -> 100 for now
    Vector3f acc = ins.get_accel() - acc_offset;
    Vector3f gyr = ins.get_gyro();
    Vector3f filtered_acc;

    //apply a low pass filter    
   // filtered_acc.x = filt_x.apply(acc.x);
   // filtered_acc.y = filt_y.apply(acc.y);
   // filtered_acc.z = filt_z.apply(acc.z);

    //turn off the filtering
    filtered_acc.x = acc.x;
    filtered_acc.y = acc.y;
    filtered_acc.z = acc.z;


    //normalize (length = 1)
    Vector3f down = filtered_acc.normalized(); // may break in free fall...

    Vector3f err = down + cntrl_up;

    //take appropriate componenets and scale
    int roll_actual = (-1)*down.y*CNTRL_RANGE;
    int pitch_actual = down.x*CNTRL_RANGE;
    int throttle_actual = cntrl_throttle; //TODO: change this out with some f(height)
    int yaw_actual = (-1)*gyr.z*YAW_SCALE;                  //this may require some scaling

    Vector3f gyr_err = (gyr)*gyrErrScale;

    int int_cntrl_roll      = cntrl_up.y*CNTRL_RANGE;
    int int_cntrl_pitch     = cntrl_up.x*CNTRL_RANGE;
    int int_cntrl_yaw       = cntrl_yaw*CNTRL_RANGE;
    int int_cntrl_throttle  = cntrl_throttle;

    int roll_error     =  err.y*CNTRL_RANGE;
    int pitch_error    =  err.x*CNTRL_RANGE;
    int throttle_error =  cntrl_throttle - throttle_actual;
    int yaw_error      =  int_cntrl_yaw - yaw_actual;
    int pitch_pid_err  =  pid_pitch->get_pid(pitch_error,dt);
    int roll_pid_err   =  pid_roll->get_pid(roll_error,dt);

    int r_correction = error_scale * (roll_pid_err - int(gyr_err.x));
    int p_correction = error_scale * (pitch_pid_err + int(gyr_err.y));
    int t_correction = error_scale * pid_throttle->get_pid(throttle_error, dt);
    int y_correction = error_scale * pid_yaw->get_pid(yaw_error, dt);

    int roll_out = int_cntrl_roll + r_correction;
    int pitch_out = int_cntrl_pitch - p_correction;
    int throttle_out = int_cntrl_throttle + t_correction;
    int yaw_out = int_cntrl_yaw - y_correction;

    //adjust motor outputs approrpiately for the pid term
    int roll_pulse = roll_out;
    int pitch_pulse = pitch_out;
    int throttle_pulse = throttle_out;
    int yaw_pulse = yaw_out;

    m_roll->servo_out = roll_pulse;
    m_pitch->servo_out = pitch_pulse;
    m_throttle->servo_out = throttle_pulse;
    m_yaw->servo_out = yaw_pulse;

    motors->output();
}

void Flight_Control::debug(int d){

}
