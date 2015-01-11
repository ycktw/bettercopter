/**
	This file is part of martink project.

	martink firmware project is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	martink firmware is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with martink firmware.  If not, see <http://www.gnu.org/licenses/>.

	Author: Martin K. Schröder
	Email: info@fortmax.se
	Github: https://github.com/mkschreder
*/

#include <stdio.h>

extern "C" {
#include <math.h>
}

#include <kernel.h>

#ifdef CONFIG_NATIVE
#include "simulator/sim_kernel.h"
#endif

#include "FlightController.hpp"

#define ALT_PID_KP 1.8 // altitude 
#define ALT_PID_KI 0.018
#define ALT_PID_KD 30.0
#define ALT_PID_MAX 20.0

extern "C" double atan2(double x, double y); 

FlightController::FlightController(){
	mMode = MODE_STABILIZE; 
}

void FlightController::Reset(){
	//mAltHoldCtrl.SetAltitude(raw_altitude); 
	mStabCtrl.Reset(); 
}

ThrottleValues FlightController::ComputeThrottle(float dt, const RCValues &rc,  
		const glm::vec3 &acc, const glm::vec3 &gyr, const glm::vec3 &mag,
		float altitude
	){
	
	//frame_log("{\"frame_log\": {"); 
	
	// frame time in seconds, prevent zero
	if(dt < 0.000001) dt = 0.000001; 
	
	uint16_t exp_thr = rc.throttle; 
	
	// use log curve for throttle (adjust for values between 1000 to 2000)
	if(rc.throttle >= 1000 && rc.throttle <= 2000){
		// map 1000-2000 range into 0-1; 
		float x = ((float)rc.throttle-1000.0)/1000.0; 
		// calculate curve and map back to 1000-2000
		exp_thr = (uint16_t)(2000.0+(log(x)) * 500.0); // 1+log(x)/2
	}
	
	if(rc.throttle > 1050) {
		if(rc.throttle < 1300){
			mAltHoldCtrl.AdjustAltitude(-0.1); 
		} else if(rc.throttle > 1700){
			mAltHoldCtrl.AdjustAltitude(0.1); 
		}
	}
	
	ThrottleValues stab = mStabCtrl.ComputeThrottle(dt, rc, acc, gyr); 
	ThrottleValues althold = mAltHoldCtrl.ComputeThrottle(altitude); 
	
	ThrottleValues throttle = ThrottleValues(MINCOMMAND); 
	
	// compute final motor throttle
	if(mMode == MODE_ALT_HOLD){
		throttle = althold + stab; 
	} else if(mMode == MODE_STABILIZE){
		throttle = ThrottleValues(exp_thr) + stab; 
	}
	
	for(int c = 0; c < 4; c++){
		throttle[c] = constrain(throttle[c], PWM_MIN, PWM_MAX); 
	}
	
	return throttle; 
	/*
	frame_log("\"raw_acc_x\": %-4d, \"raw_acc_y\": %-4d, \"raw_acc_z\": %-4d, ", 
		(int16_t)(acc.x * 100), (int16_t)(acc.y * 100), (int16_t)(acc.z * 100)); 
	frame_log("\"raw_gyr_x\": %-4d, \"raw_gyr_y\": %-4d, \"raw_gyr_z\": %-4d, ", 
		(int16_t)(gyr.x * 100), (int16_t)(gyr.y * 100), (int16_t)(gyr.z * 100)); 
	frame_log("\"raw_temp\": %-4d, \"raw_press\": %ld, \"raw_alt\": %-4d, ", 
		(int16_t)(temp * 100), pressure, (int16_t)(altitude * 100)); 
	
	frame_log("\"rc\": [%-4d, %-4d, %-4d, %-4d], ", 
		(uint16_t)rc.throttle, 
		(uint16_t)rc.yaw, 
		(uint16_t)rc.pitch, 
		(uint16_t)rc.roll, 
		(uint16_t)rc.aux0); 
		
	frame_log("\"out_thr\": [%-4d, %-4d, %-4d, %-4d],",
		throttle[0], 
		throttle[1], 
		throttle[2], 
		throttle[3]
	); 
	frame_log("\"dummy\": \"\"}}\n"); 
	*/
}
