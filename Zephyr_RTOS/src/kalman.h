/*
Credits: TKJElectronics
Referred from "https://github.com/TKJElectronics/KalmanFilter/blob/master/Kalman.cpp"
*/

#ifndef _Kalman_h_
#define _Kalman_h_

struct kalman{
	double Q_angle; // Process noise variance for the accelerometer
    double Q_bias; // Process noise variance for the gyro bias
    double R_measure; // Measurement noise variance - this is actually the variance of the measurement noise

    double angle; // The angle calculated by the Kalman filter - part of the 2x1 state vector
    double bias; // The gyro bias calculated by the Kalman filter - part of the 2x1 state vector
    double rate; // Unbiased rate calculated from the rate and the calculated bias - you have to call getAngle to update the rate

    float P[2][2]; // Error covariance matrix - This is a 2x2 matrix
    double K[2];
    double y;
    double S;
};

double kalman_angle(double newAngle, double newRate, double dt, struct kalman *var){

	var->rate = newRate - var->bias;
    var->angle += dt * var->rate;

    // Update estimation error covariance - Project the error covariance ahead
    var->P[0][0] += dt * (dt*(var->P[1][1]) - var->P[0][1] - var->P[1][0] + var->Q_angle);
    var->P[0][1] -= dt * var->P[1][1];
    var->P[1][0] -= dt * var->P[1][1];
    var->P[1][1] += var->Q_bias * dt;

    // Discrete Kalman filter measurement update equations - Measurement Update ("Correct")
    // Calculate Kalman gain - Compute the Kalman gain
    var->S = var->P[0][0] + var->R_measure; // Estimate error
    var->K[0] = var->P[0][0] / var->S;
    var->K[1] = var->P[1][0] / var->S;

    // Calculate angle and bias - Update estimate with measurement zk (newAngle)
    var->y = newAngle - var->angle; // Angle difference

    var->angle += var->K[0] * var->y;
    var->bias += var->K[1] * var->y;

    // Calculate estimation error covariance - Update the error covariance
    var->P[0][0] -= var->K[0] * var->P[0][0];
    var->P[0][1] -= var->K[0] * var->P[0][1];
    var->P[1][0] -= var->K[1] * var->P[0][0];
    var->P[1][1] -= var->K[1] * var->P[0][1];

    return var->angle;
}

#endif
