/* linear motion calcs for accel/vel experiments */

float lin_b_accel = 0.3; /* braking acceleration magnitude */
float lin_a0 = 0.05; /* acceleration for constant-a test */
float lin_v0 = 0.03; /* velocity for constant-v test */
float lin_ai = 0.3; /* initial acceleration for constant-v test */
float lin_rl = 0.45; /* travelling limit */
float lin_ti, lin_ts, lin_te;    /* calculated as needed */

#define T_CACC 0
#define T_CVEL 1

int lin_trial = -1;

/* There are two test types - one is constant accel, the other is
   constant velocity.  For constant accel, v(0) == 0, r(0) == 0,
   ts = time that braking starts
   te = time that motion ends
   v(te) == 0,
   r(te) == rl
*/

/* calculates ts */
float ca_braking_time(void) {
  return sqrt((2*lin_b_accel*lin_rl)/(lin_a0*(lin_b_accel+lin_a0)));
}

float ca_finish_time(void) {
  return lin_a0*ca_braking_time()/lin_b_accel + ca_braking_time();
}

int new_ca_trial(float ca) {
  lin_a0 = ca;
  lin_ti = 0.0;
  lin_ts = ca_braking_time();
  lin_te = ca_finish_time();
  lin_trial = T_CACC;
  if (lin_ts < 0.0 || lin_ts >= lin_te) {
    fprintf(stderr, "ca trial %f cannot be run within motion limits\n",ca);
    exit(1);
  }
  return 0;
}

/* determine acceleration at time t */ 
float ca_acc(float t) {
  if (t < lin_ts)
    return lin_a0;
  else if (t < lin_te)
    return -lin_b_accel;
  else
    return 0.0;
}

/* determine velocity at time t */
float ca_vel(float t) {
  if (t < lin_ts)
    /* constant-a mode */
    return lin_a0*t;
  else if (t < lin_te) {
    /* breaking mode */
    return lin_a0*lin_ts - lin_b_accel*(t-lin_ts);
  } else {
    return 0.0;
  }
}

/* determine position at time t */
float ca_pos(float t) {
  if (t < lin_ts)
    return 0.5*lin_a0*t*t;
  else if (t < lin_te)
    return 0.5*lin_a0*lin_ts*lin_ts + lin_a0*lin_ts*(t-lin_ts) - 0.5*lin_b_accel*(t-lin_ts)*(t-lin_ts);
  else
    return lin_rl;
}

/*** constant v calculations */
float cv_init_time(void) {
  return lin_v0/lin_ai;
}

float cv_braking_time(void) {
  float t;
  t = cv_init_time();
  return (lin_rl - 0.5*lin_ai*t*t)/lin_v0 + lin_v0/(2*lin_ai) - (3*lin_v0)/2*lin_b_accel;
}

float cv_finish_time(void) {
  return cv_braking_time() + lin_v0/lin_b_accel;
}

int new_cv_trial(float cv) {
  lin_v0 = cv;
  lin_ti = cv_init_time();
  lin_ts = cv_braking_time();
  lin_te = cv_finish_time();
  lin_trial = T_CVEL;
  /* check for valid trial */
  if (lin_ti < 0 || lin_ti >= lin_ts || lin_ts >= lin_te) {
    fprintf(stderr, "cv trial %f cannot be run within motion limits\n",cv);
    exit(1);
  }
  return 0;
}

float cv_acc(float t) {
  if (t < lin_ti)
    return lin_ai;
  else if (t < lin_ts)
    return 0.0;
  else if (t < lin_te)
    return -lin_b_accel;
  else
    return 0.0;
}

float cv_vel(float t) {
  if (t < lin_ti)
    return lin_ai*t;
  else if (t < lin_ts)
    return lin_v0;
  else if (t < lin_te)
    return lin_v0 - lin_b_accel*(t-lin_ts);
  else
    return 0.0;
}

float cv_pos(float t) {
  if (t < lin_ti)
    return 0.5*lin_ai*t*t;
  else if (t < lin_ts)
    return lin_v0*t - lin_v0*lin_v0/(2*lin_ai);
  else {
    /* stop time at te - beyond te, pos should just be lin_rl */
    if (t > lin_te)
      t = lin_te;
    return lin_v0*t - lin_v0*lin_v0/(2*lin_ai) - 0.5*lin_b_accel*(t-lin_ts)*(t-lin_ts);
  }
}

float trial_acc(float t) {
  return (lin_trial == T_CVEL) ? cv_acc(t) : ca_acc(t);
}

float trial_vel(float t) {
  return (lin_trial == T_CVEL) ? cv_vel(t) : ca_vel(t);
}

float trial_pos(float t) {
  return (lin_trial == T_CVEL) ? cv_pos(t) : ca_pos(t);
}

int new_trial(int type, float c) {
  if (type == T_CVEL)
    return new_cv_trial(c);
  else if (type == T_CACC)
    return new_ca_trial(c);
  else
    return -1;
}
