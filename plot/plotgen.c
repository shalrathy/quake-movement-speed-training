#include <stdio.h>
#include <math.h>
#include <string.h>

#define JUMPTIME 0.64

#define	PITCH		0
#define	YAW		1
#define	ROLL		2

#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}

#define	MOVETYPE_WALK			3		// gravity
#define	MOVETYPE_NOCLIP			8

typedef float	vec_t;
typedef vec_t	vec3_t[3];
typedef enum {
	false = 0,
	true  = 1
} qboolean;
typedef struct cvar_s {
    float value;
} cvar_t;

typedef struct {
    vec3_t angles;
    vec3_t mins;
    float teleport_time;
    float movetype;
} entvars_t;
typedef struct edict_s {
    entvars_t v;
} edict_t;
edict_t sv_player0;
edict_t *sv_player = &sv_player0;

typedef struct {
    float forwardmove;
    float sidemove;
    float upmove;
} cmd_t;
cmd_t cmd = {1, 2, 3};

typedef struct {
    double time;
} server_t; 
server_t sv = {0};

vec3_t vec3_origin = {0,0,0};
vec3_t velocity = {0, 0, 0};
vec3_t origin = {0, 0, 0};
vec3_t forward, right, up;
double host_frametime = 0.014; // 0.015
qboolean onground = true;

cvar_t sv_maxspeed = {320};
cvar_t sv_accelerate = {10};
cvar_t sv_stopspeed = {100};
cvar_t sv_friction = {4};
cvar_t sv_edgefriction = {2};
cvar_t sv_gravity = {800};
float ent_gravity = 1.0f;

void init() {
    sv_player->v.movetype = MOVETYPE_WALK;
    sv_player->v.mins[0] = sv_player->v.mins[1] = sv_player->v.mins[2] = 0;
    sv_player->v.angles[0] = sv_player->v.angles[1] = sv_player->v.angles[2] = 0;
    sv_player->v.teleport_time = 0;
    onground = true;
    cmd.forwardmove = 400;
    //cmd.forwardmove = 800; // always run on
    cmd.sidemove = 700;
    //cmd.sidemove = -700;
}

void VectorScale (vec3_t in, vec_t scale, vec3_t out) {
    out[0] = in[0]*scale;
    out[1] = in[1]*scale;
    out[2] = in[2]*scale;
}
float VectorNormalize (vec3_t v) {
    float length, ilength;
    length = sqrt(DotProduct(v,v));
    if (length) {
        ilength = 1/length;
        v[0] *= ilength;
        v[1] *= ilength;
        v[2] *= ilength;
    }
    return length;
}

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
    float		angle;
    float		sr, sp, sy, cr, cp, cy;

    angle = angles[YAW] * (M_PI*2 / 360);
    sy = sin(angle);
    cy = cos(angle);
    angle = angles[PITCH] * (M_PI*2 / 360);
    sp = sin(angle);
    cp = cos(angle);
    angle = angles[ROLL] * (M_PI*2 / 360);
    sr = sin(angle);
    cr = cos(angle);

    forward[0] = cp*cy;
    forward[1] = cp*sy;
    forward[2] = -sp;
    right[0] = (-1*sr*sp*cy+-1*cr*-sy);
    right[1] = (-1*sr*sp*sy+-1*cr*cy);
    right[2] = -1*sr*cp;
    up[0] = (cr*sp*cy+-sr*-sy);
    up[1] = (cr*sp*sy+-sr*cy);
    up[2] = cr*cp;
}

void SV_AirAccelerate (float wishspeed, vec3_t wishveloc) {
    int i;
    float addspeed, wishspd, accelspeed, currentspeed;

    wishspd = VectorNormalize (wishveloc);
    if (wishspd > 30)
        wishspd = 30;
    currentspeed = DotProduct (velocity, wishveloc);
    addspeed = wishspd - currentspeed;
    if (addspeed <= 0)
        return;
//	accelspeed = sv_accelerate.value * host_frametime;
    accelspeed = sv_accelerate.value*wishspeed * host_frametime;
    /* printf("## wishspeed=%f, wishvel=%d,%d,%d, wishspd=%f, currentspeed=%f, addspeed=%f, accelspeed=%f, sv_accelerate=%f\n", */
    /*        wishspeed, (int)wishveloc[0], (int)wishveloc[1], (int)wishveloc[2], */
    /*        wishspd, currentspeed, addspeed, accelspeed, sv_accelerate.value); */
           
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    for (i=0 ; i<3 ; i++)
        velocity[i] += accelspeed*wishveloc[i];
}

void SV_Accelerate (float wishspeed, const vec3_t wishdir) {
    int	i;
    float addspeed, accelspeed, currentspeed;

    currentspeed = DotProduct (velocity, wishdir);
    addspeed = wishspeed - currentspeed;
    if (addspeed <= 0)
        return;
    accelspeed = sv_accelerate.value*host_frametime*wishspeed;
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    for (i=0 ; i<3 ; i++)
        velocity[i] += accelspeed*wishdir[i];
}

typedef struct {
    float fraction;
} trace_t;
trace_t SV_Move (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict) {
    trace_t trace;
    trace.fraction = 0;
    return trace;
}

void SV_UserFriction (void) {
    float *vel;
    float speed, newspeed, control;
    vec3_t start, stop;
    float friction;
    trace_t trace;

    vel = velocity;

    speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1]);
    if (!speed)
        return;

// if the leading edge is over a dropoff, increase friction
    start[0] = stop[0] = origin[0] + vel[0]/speed*16;
    start[1] = stop[1] = origin[1] + vel[1]/speed*16;
    start[2] = origin[2] + sv_player->v.mins[2];
    stop[2] = start[2] - 34;

    trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

    if (trace.fraction == 1.0)
        friction = sv_friction.value*sv_edgefriction.value;
    else
        friction = sv_friction.value;

// apply friction
    control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
    newspeed = speed - host_frametime*control*friction;

    if (newspeed < 0)
        newspeed = 0;
    newspeed /= speed;

    vel[0] = vel[0] * newspeed;
    vel[1] = vel[1] * newspeed;
    vel[2] = vel[2] * newspeed;
}

void SV_AirMove (void) {
    int	i;
    vec3_t wishvel, wishdir;
    float wishspeed;
    float fmove, smove;

    AngleVectors (sv_player->v.angles, forward, right, up);

    fmove = cmd.forwardmove;
    smove = cmd.sidemove;

// hack to not let you back into teleporter
    if (sv.time < sv_player->v.teleport_time && fmove < 0)
        fmove = 0;

    for (i=0 ; i<3 ; i++)
        wishvel[i] = forward[i]*fmove + right[i]*smove;

    if ( (int)sv_player->v.movetype != MOVETYPE_WALK)
        wishvel[2] = cmd.upmove;
    else
        wishvel[2] = 0;

    VectorCopy (wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    if (wishspeed > sv_maxspeed.value) {
        VectorScale (wishvel, sv_maxspeed.value/wishspeed, wishvel);
        wishspeed = sv_maxspeed.value;
    }
    if (sv_player->v.movetype == MOVETYPE_NOCLIP) {	// noclip
        VectorCopy (wishvel, velocity);
    }
    // emulate qw bunny hopping by not applying friction on first landing tick
    else if (onground){
        SV_UserFriction ();
        SV_Accelerate (wishspeed, wishdir);
    } else { // not on ground, so little effect on velocity
        SV_AirAccelerate (wishspeed, wishvel);
    }
}

float getSpeedAdd() {
    if (sv_player->v.angles[1] > 180) sv_player->v.angles[1] - 360;
    if (sv_player->v.angles[1] < -180) sv_player->v.angles[1] + 360;
    
    vec3_t oldvelocity;
    for (int i=0;i<3;i++) oldvelocity[i] = velocity[i];
    float oldspeed = sqrt(velocity[0]*velocity[0] +velocity[1]*velocity[1]);
    SV_AirMove();
    float newspeed = sqrt(velocity[0]*velocity[0] +velocity[1]*velocity[1]);
    for (int i=0;i<3;i++) velocity[i] = oldvelocity[i];
    return newspeed - oldspeed;
}

void tick() {
    if (!onground)
        velocity[2] -= ent_gravity * sv_gravity.value * host_frametime;
    SV_AirMove();
    for (int i=0;i<3;i++) origin[i] += velocity[i];
    if (origin[2] < 0) origin[2] = 0;
    sv.time += host_frametime;
}

void stabilize() {
    float speed = sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);
    while (true) {
        printf("speed d=%f, %.2f, %.2f, %.2f\n",
               speed, velocity[0], velocity[1], velocity[2]);
        SV_AirMove();

        float newspeed = sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);
        if (newspeed == speed) break;
        speed = newspeed;
    }
}

void test() {
    int speedfrom = 400;
    int speedto = 400;
    int anglefrom = -90;
    int angleto = 90;

    for (int speed = speedfrom; speed <= speedto; speed++) {
        for (int angle = anglefrom; angle <= angleto; angle++) {
            velocity[0] = speed;
            sv_player->v.angles[1] = angle; // left-right look
            /* float addspeedleft = getSpeedAdd(); */
            cmd.forwardmove = 0; // +speed always run off
            //cmd.forwardmove = 200; // walk
            //cmd.forwardmove = 400; // +speed always run off
            //cmd.forwardmove = 800; // +speed always run on
            cmd.sidemove = 700;
            //cmd.sidemove = -700;
            onground = false;
            float addspeed1 = getSpeedAdd();

            cmd.sidemove = -700;
            float addspeed2 = getSpeedAdd();
            printf("speed=%d, angle=%d, addspeed1=%.2f addspeed2=%.2f\n",
                   speed, angle, addspeed1, addspeed2);
        }
    }
}

float bestAngle() {
    float bestangle = 0;
    sv_player->v.angles[1] = bestangle;
    float bestspeed = getSpeedAdd();

    for (int i = 1; i < 45; i++) {
        for (int n = -1; n <= 1; n+=2) {
            sv_player->v.angles[1] = i*n;
            float speed = getSpeedAdd();
            if (speed > bestspeed) {
                bestangle = sv_player->v.angles[1];
                bestspeed = speed;
            }
        }
    }

    for (int i = 1; i < 1000; i++) {
        for (int n = -1; n <= 1; n+=2) {
            sv_player->v.angles[1] = n * i/100.0f;
            float speed = getSpeedAdd();
            if (speed > bestspeed) {
                bestangle = sv_player->v.angles[1];
                bestspeed = speed;
            }
        }
    }

    float diff = 1.0f;
    while (diff > 0.0001f) {
        for (int n = -1; n <= 2; n+=2) {
            while (true) {
                sv_player->v.angles[1] = bestangle + n*diff;
                float newspeed = getSpeedAdd();
                if (newspeed > bestspeed) {
                    bestangle = sv_player->v.angles[1];
                    bestspeed = newspeed;
                } else {
                    break;
                }
            }
        }
        diff /= 10.0f;
    }

    return bestangle;
}

void plot(char *file, const char *name) {
    FILE *f = fopen(file, "w");
    
    int speedfrom = 0;
    int speedto = 700;
    int anglefrom = -180;
    int angleto = 180;
    float angleinc = 1;

    fprintf(f, "<html><head><script src=https://cdn.plot.ly/plotly-2.12.1.min.js></script></head><body>\n");
    fprintf(f, "<div id=plot></div><script>\n");
    fprintf(f, "var xs = [");
    for (float angle = anglefrom; angle <= angleto; angle+=angleinc) {
        fprintf(f, "'%.2f',", angle);
    }
    fprintf(f, "];");
    fprintf(f, "var ys = [");
    for (int speed = speedfrom; speed <= speedto; speed++) {
        fprintf(f, "'%d',", speed);
    }
    fprintf(f, "];");

    float minspeed = 0;
    float maxspeed = 0;
    
    fprintf(f, "var zs = [\n");
    for (int speed = speedfrom; speed <= speedto; speed++) {
        fprintf(f, "[");
        for (float angle = anglefrom; angle <= angleto; angle+=angleinc) {
            velocity[0] = speed;
            sv_player->v.angles[1] = angle; // left-right look
            float addspeed = getSpeedAdd();
            fprintf(f, "%.4f,", addspeed);
            if (addspeed < minspeed) minspeed = addspeed;
            if (addspeed > maxspeed) maxspeed = addspeed;
        }
        fprintf(f, "],\n");
    }
    fprintf(f, "];\n");
    float mid = fabs(minspeed)/(fabs(minspeed)+fabs(maxspeed));
    if (!isfinite(mid)) mid = 0.5;
    //fprintf(f, "var cols = [[0, '#FF0000'], [%.2f, '#FFFFFF'], [1.0, '#00FF00']];\n", mid);
    //fprintf(f, "var cols = [[0, '#FF0000'], [%.2f, '#FFFFAA'], [1.0, '#004400']];\n", mid);
    fprintf(f, "var cols = [[0, '#440000'], [%.2f, '#EE0000'], [%.2f, '#FFFFAA'], [%.2f, '#00EE00'], [1.0, '#004400']];\n",
            fmax(0,mid-0.05), mid, fmin(1.0, mid+0.05));
    //fprintf(f, "var cols = [[0, '#FFFFFF'], [%.2f, '#FFFFFF'], [1.0, '#00FF00']];\n", mid);
    //fprintf(f, "var cols = [[0, '#FF0000'], [0.5, '#FFFFAA'], [1.0, '#004400']];\n");
    fprintf(f, "var data = [{name:'%s', x:xs, y:ys, z:zs, type:'heatmap', colorscale:cols, ", name);
    fprintf(f, "showscale:true, opacity:0.8}];\n");
    //fprintf(f, "showscale:true, opacity:0.8, zmin:-1, zmax:1}];\n");
    fprintf(f, "Plotly.newPlot('plot', data, {autosize:true, width:900, height:900, title:{text:'%s'}, ", name);
    fprintf(f, "xaxis:{title:'view angle away from move direction',gridcolor:'#000'}, ");
    fprintf(f, "yaxis:{title:'speed',gridcolor:'#000'}},");
    char pngname[100];
    strcpy(pngname, file);
    pngname[strlen(pngname)-strlen(".html")] = 0;
    fprintf(f, "{toImageButtonOptions:{filename:'%s'}});\n", pngname);
    fprintf(f, "</script></body></html>\n");

    fclose(f);
}

void plot_best_angle(char *file, const char *name, int strafeleft) {
    FILE *f = fopen(file, "w");
    
    int speedfrom = 0;
    int speedto = 700;
    float speedinc = 0.1;
    int anglefrom = -45;
    int angleto = 45;
    float angleinc = 0.1;

    fprintf(f, "<html><head><script src=https://cdn.plot.ly/plotly-2.12.1.min.js></script></head><body>\n");
    fprintf(f, "<div id=plot></div><script>\n");
    fprintf(f, "var ys = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        fprintf(f, "'%.2f',", speed);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xs = [");
    onground = true;
    velocity[0] = velocity[1] = velocity[2] = 0;
    // move forward with always run off and strafe right
    cmd.forwardmove = 400;
    cmd.sidemove = strafeleft ? -700 : 700;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        float angle = bestAngle();
        fprintf(f, "'%.2f',", angle);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsl1 = [");
    velocity[0] = velocity[1] = velocity[2] = 0;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedadd_target = getSpeedAdd() - 5;
        while (getSpeedAdd() > speedadd_target && sv_player->v.angles[1] > -45) {
            sv_player->v.angles[1] -= 0.1;
        }
        fprintf(f, "'%.2f',", sv_player->v.angles[1]);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsh1 = [");
    velocity[0] = velocity[1] = velocity[2] = 0;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedadd_target = getSpeedAdd() - 5;
        while (getSpeedAdd() > speedadd_target && sv_player->v.angles[1] < 45) {
            sv_player->v.angles[1] += 0.1;
        }
        fprintf(f, "'%.2f',", sv_player->v.angles[1]);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsl2 = [");
    velocity[0] = velocity[1] = velocity[2] = 0;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedadd_target = getSpeedAdd() - 10;
        while (getSpeedAdd() > speedadd_target && sv_player->v.angles[1] > -45) {
            sv_player->v.angles[1] -= 0.1;
        }
        fprintf(f, "'%.2f',", sv_player->v.angles[1]);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsh2 = [");
    velocity[0] = velocity[1] = velocity[2] = 0;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedadd_target = getSpeedAdd() - 10;
        while (getSpeedAdd() > speedadd_target && sv_player->v.angles[1] < 45) {
            sv_player->v.angles[1] += 0.1;
        }
        fprintf(f, "'%.2f',", sv_player->v.angles[1]);
    }
    fprintf(f, "];\n");

    fprintf(f, "var data = [];\n");
    fprintf(f, "data.push({name:'left angle -10 speed', x:xsl2, y:ys, type:'scatter',line:{dash:'dot',color:'#EE0000'}});\n");
    fprintf(f, "data.push({name:'left angle -5 speed', x:xsl1, y:ys, type:'scatter',line:{dash:'dot',color:'#777700'}});\n");
    fprintf(f, "data.push({name:'best angle', x:xs, y:ys, type:'scatter',line:{color:'#00EE00'}});\n");
    fprintf(f, "data.push({name:'right angle -5 speed', x:xsh1, y:ys, type:'scatter',line:{dash:'dash',color:'#777700'}});\n");
    fprintf(f, "data.push({name:'right angle -10 speed', x:xsh2, y:ys, type:'scatter',line:{dash:'dash',color:'#EE0000'}});\n");
    fprintf(f, "Plotly.newPlot('plot', data, {autosize:true, width:900, height:900, title:{text:'%s'}, ", name);
    fprintf(f, "xaxis:{title:'view angle away from move direction',gridcolor:'#000',range:[-45,45],autorange:false}, ");
    fprintf(f, "yaxis:{title:'speed',gridcolor:'#000'}},");
    char pngname[100];
    strcpy(pngname, file);
    pngname[strlen(pngname)-strlen(".html")] = 0;
    fprintf(f, "{toImageButtonOptions:{filename:'%s'}});\n", pngname);
    fprintf(f, "</script></body></html>\n");

    fclose(f);
}

void plot_best_angle_speedadd(char *file, const char *name, int strafeleft) {
    FILE *f = fopen(file, "w");
    
    int speedfrom = 0;
    int speedto = 700;
    float speedinc = 0.1;
    int anglefrom = -45;
    int angleto = 45;
    float angleinc = 0.1;

    fprintf(f, "<html><head><script src=https://cdn.plot.ly/plotly-2.12.1.min.js></script></head><body>\n");
    fprintf(f, "<div id=plot></div><script>\n");
    fprintf(f, "var ys = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        fprintf(f, "'%.2f',", speed);
    }
    fprintf(f, "];\n");
    // move forward with always run off and strafe right
    onground = true;
    cmd.forwardmove = 400;
    cmd.sidemove = strafeleft ? -700 : 700;
    fprintf(f, "var xs = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedadd = getSpeedAdd();
        fprintf(f, "'%.2f',", speedadd);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsl1 = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle() - 5;
        float speedadd = getSpeedAdd();
        fprintf(f, "'%.2f',", speedadd);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsh1 = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle() + 5;
        float speedadd = getSpeedAdd();
        fprintf(f, "'%.2f',", speedadd);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsl2 = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle() - 10;
        float speedadd = getSpeedAdd();
        fprintf(f, "'%.2f',", speedadd);
    }
    fprintf(f, "];\n");
    fprintf(f, "var xsh2 = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle() + 10;
        float speedadd = getSpeedAdd();
        fprintf(f, "'%.2f',", speedadd);
    }
    fprintf(f, "];\n");

    float minspeed = 0;
    float maxspeed = 0;

    fprintf(f, "var data = [];\n");
    fprintf(f, "data.push({name:'best left angle -10 speed', x:xsl2, y:ys, type:'scatter',line:{dash:'dot',color:'#EE0000'}});\n");
    fprintf(f, "data.push({name:'best left angle -5 speed', x:xsl1, y:ys, type:'scatter',line:{dash:'dot',color:'#777700'}});\n");
    fprintf(f, "data.push({name:'best angle speed', x:xs, y:ys, type:'scatter'});\n");
    fprintf(f, "data.push({name:'best right angle -5 speed', x:xsh1, y:ys, type:'scatter',line:{dash:'dash',color:'#777700'}});\n");
    fprintf(f, "data.push({name:'best right angle -10 speed', x:xsh2, y:ys, type:'scatter',line:{dash:'dash',color:'#EE0000'}});\n");
    fprintf(f, "Plotly.newPlot('plot', data, {autosize:true, width:900, height:900, title:{text:'%s'}, ", name);
    fprintf(f, "xaxis:{title:'speed change',gridcolor:'#000'}, ");
    fprintf(f, "yaxis:{title:'speed',gridcolor:'#000'}},");
    char pngname[100];
    strcpy(pngname, file);
    pngname[strlen(pngname)-strlen(".html")] = 0;
    fprintf(f, "{toImageButtonOptions:{filename:'%s'}});\n", pngname);
    fprintf(f, "</script></body></html>\n");

    fclose(f);
}

void plot_best_angle_and_addspeed(char *file, char *name) {
    FILE *f = fopen(file, "w");
    
    int speedfrom = 320;
    int speedto = 700;
    float speedinc = 0.1;
    int anglefrom = -45;
    int angleto = 45;
    float angleinc = 0.1;

    fprintf(f, "<html><head><script src=https://cdn.plot.ly/plotly-2.12.1.min.js></script></head><body>\n");
    fprintf(f, "<p>%s</p>\n", name);
    fprintf(f, "<div id=plot></div><script>\n");
    fprintf(f, "var xs = [");
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        fprintf(f, "'%.2f',", speed);
    }
    fprintf(f, "];\n");
    fprintf(f, "var ys1 = [");
    onground = true;
    velocity[0] = velocity[1] = velocity[2] = 0;
    // move forward with always run off and strafe right
    cmd.forwardmove = 400;
    cmd.sidemove = 700;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        float angle = bestAngle();
        fprintf(f, "'%.2f',", angle);
    }
    fprintf(f, "];\n");
    fprintf(f, "var ys1l = [");
    onground = true;
    velocity[0] = velocity[1] = velocity[2] = 0;
    // move forward with always run off and strafe right
    cmd.forwardmove = 400;
    cmd.sidemove = 700;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedlow = getSpeedAdd() * 0.8;
        while (getSpeedAdd() > speedlow) {
            sv_player->v.angles[1] -= 0.01;
        }
        fprintf(f, "'%.2f',", sv_player->v.angles[1]);
    }
    fprintf(f, "];\n");
    fprintf(f, "var ys1r = [");
    onground = true;
    velocity[0] = velocity[1] = velocity[2] = 0;
    // move forward with always run off and strafe right
    cmd.forwardmove = 400;
    cmd.sidemove = 700;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedlow = getSpeedAdd() * 0.8;
        while (getSpeedAdd() > speedlow) {
            sv_player->v.angles[1] += 0.01;
        }
        fprintf(f, "'%.2f',", sv_player->v.angles[1]);
    }
    fprintf(f, "];\n");
    fprintf(f, "var ys2 = [");
    onground = true;
    velocity[0] = velocity[1] = velocity[2] = 0;
    // move forward with always run off and strafe right
    cmd.forwardmove = 400;
    cmd.sidemove = 700;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = bestAngle();
        float speedadd = getSpeedAdd();
        fprintf(f, "'%.2f',", speedadd);
    }
    fprintf(f, "];\n");
    fprintf(f, "var ys2fwd = [");
    onground = true;
    velocity[0] = velocity[1] = velocity[2] = 0;
    // move forward with always run off and strafe right
    cmd.forwardmove = 400;
    cmd.sidemove = 700;
    for (float speed = speedfrom; speed <= speedto; speed += speedinc) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = speed;
        sv_player->v.angles[1] = 0;
        float speedadd = getSpeedAdd();
        fprintf(f, "'%.2f',", speedadd);
    }
    fprintf(f, "];\n");

    // todo: add lines showing best angle 90% speed, and 80% speed

    float minspeed = 0;
    float maxspeed = 0;
    
    fprintf(f, "var data = [{name:'best view angle', x:ys1, y:xs, type:'scatter'},\n");
    fprintf(f, "            {name:'best view angle 90%% speed left', x:ys1l, y:xs, type:'scatter'},\n");
    fprintf(f, "            {name:'best view angle 90%% speed right', x:ys1r, y:xs, type:'scatter'},\n");
    fprintf(f, "            {name:'forward view angle speed add', x:ys2fwd, y:xs, xaxis:'x2', type:'scatter'},\n");
    fprintf(f, "            {name:'best view angle speed add', x:ys2, y:xs, xaxis:'x2', type:'scatter'},];\n");
    fprintf(f, "Plotly.newPlot('plot', data, {autosize:true, width:900, height:900, \n");
    fprintf(f, "xaxis:{title:{text:'best view angle',font:{color:'#1f77b4'}},gridcolor:'#000',range:[-30,30]}, \n");
    fprintf(f, "yaxis:{title:'speed',gridcolor:'#000'},\n");
    fprintf(f, "xaxis2:{title:{text:'best view angle speed add',font:{color:'#ff7f0e'}},overlaying:'x',side:'top',gridcolor:'#000',range:[-30,30]}},\n");
    char pngname[100];
    strcpy(pngname, file);
    pngname[strlen(pngname)-strlen(".html")] = 0;
    fprintf(f, "{toImageButtonOptions:{filename:'%s'}});\n", pngname);
    fprintf(f, "</script></body></html>\n");

    fclose(f);
}


int main() {
    init();
    //velocity[1] = -320;
    //sv_player->v.angles[1] = 0; // left-right look
    //stabilize();

    //onground = false;
    onground = true;
    //cmd.forwardmove = 0; // stand
    //cmd.forwardmove = 200; // walk
    cmd.forwardmove = 400; // +speed always run off
    //cmd.forwardmove = 800; // +speed always run on
    //cmd.sidemove = 0;
    //cmd.sidemove = -350; // wal
    cmd.sidemove = -700;
    //cmd.sidemove = 700; 
    //stabilize();
    //SV_AirMove();
    //SV_AirMove();
    //sv_player->v.angles[0] = 30; // look up
    //plot("plot.html", "foo");
    //test();

    if (false) {
        // vel[2]=604.040649, entgravity=1.000000, sv_gravity=800.000000, frametime=0.014000, time=2.359000
        // vel[2]=8.840405, entgravity=1.000000, sv_gravity=800.000000, frametime=0.014000, time=3.103000
        // (0.014+3.103)-2.359 = 0.758

        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = 320;
        velocity[2] = 270; // jump
        onground = false;
        for (int i = 0; i < 100; i++) {
            tick();
            printf("time=%.2f, vel=%.0f,%.0f,%.0f, org=%.0f,%.0f,%.0f\n",
                   sv.time,
                   velocity[0], velocity[1], velocity[2],
                   origin[0], origin[1], origin[2]);
        }
    }

    if (false) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = 320;
        onground = false;
        cmd.forwardmove = 0;
        cmd.sidemove = -700;
        int prevsidemovechange = -100;
        int prevbestsidemove = 0;
        for (int i = 0; i <= 200; i++) {
            int bestsidemove = -700;
            float bestangle = 0;
            sv_player->v.angles[1] = bestangle;
            float bestxspeed = -99;
            for (int sidemove = -700; sidemove <= 700; sidemove+=1400) {
                cmd.sidemove = sidemove;
                for (float a = 0; a <= 45; a+=0.1f) {
                    for (int sign = -1; sign <= 1; sign+=2) {
                        sv_player->v.angles[1] = a*sign;
                        vec3_t oldvel;
                        for (int j=0;j<3;j++)oldvel[j]=velocity[j];
                        SV_AirMove();
                        float xspeed = velocity[0] - oldvel[0];
                        for (int j=0;j<3;j++)velocity[j]=oldvel[j];
                        if (xspeed > bestxspeed) {
                            if (prevbestsidemove != cmd.sidemove && i - prevsidemovechange <= 0) continue;
                            bestxspeed = xspeed;
                            bestangle = sv_player->v.angles[1];
                            bestsidemove = cmd.sidemove;
                        }
                    }
                }
            }
            if (prevbestsidemove != bestsidemove)
                prevsidemovechange = i;
            prevbestsidemove = bestsidemove;

            printf("%d: xspeed=%.2f, speed=%.2f, angle=%f, side=%d, bestxspeed=%.2f\n", i, velocity[0],
                   sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]), bestangle, bestsidemove, bestxspeed);
            sv_player->v.angles[1] = bestangle;
            cmd.sidemove = bestsidemove;
            SV_AirMove();
        }

        //100: xspeed=352.09, speed=352, angle=2.400000, side=700
        //100: xspeed=426.84, speed=427, angle=-2.700000, side=-700
    }

    
    if (false) {
        velocity[0] = velocity[1] = velocity[2] = 0;
        velocity[0] = 320;
        for (int j = 0; j < 2; j++) {
            for (int dir = 0; dir <= 1; dir++) {
                cmd.sidemove *= -1;
                for (int i = 0; i < 6; i++) {
                    sv_player->v.angles[1] = bestAngle();
                    printf("vel=%.0f,%.0f,%.0f, speed=%.0f, addspeed=%.0f, angle=%.0f, sidemove=%.0f\n",
                           velocity[0], velocity[1], velocity[2],
                           sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]),
                           getSpeedAdd(),
                           sv_player->v.angles[1], cmd.sidemove);
                    SV_AirMove();
                    if (j == 0 && dir == 0 && i == 2) break;
                }
            }
        }
    }

    if (false) {
        for (int a = 0; a < 2; a++) {
            for (int b=0;b<4;b++) {
                for (int c=0;c<3;c++) {
                    onground = a ? true : false;
                    cmd.forwardmove = b == 0 ? 0 : b == 1 ? 200 : b == 2 ? 400 : 800;
                    float sidemove = b == 1 ? 350 : 700;
                    cmd.sidemove = c == 0 ? 0 : c == 1 ? -sidemove : sidemove;
                    char n[100];
                    sprintf(n, "plot_%s_%s_%s.html",
                            onground?"ground":"air",
                            c==0?"forward":c==1?"left":"right",
                            b==0?"stand":b==1?"walk":b==2?"always_run_off":"always_run_on");
                    char n2[100];
                    sprintf(n2, "%s, %s%s",
                            onground?"Ground":"Air",
                            c==0?"":c==1?"Left, ":"Right, ",
                            b==0?"Stand":b==1?"Walk":b==2?"Run (Always run off)":"Run (Always run on)");
                    velocity[0] = velocity[1] = velocity[2] = 0;
                    //stabilize();
                    //if (!onground) SV_AirMove();
                    if (!onground) stabilize();
                    plot(n, n2);
                }
            }
        }
    }

    if (false) {
        for (int a = -30; a <= 30; a+=10) {
            for (int c=0;c<3;c++) {
                onground = true;
                cmd.forwardmove = 400;
                float sidemove = 700;
                cmd.sidemove = c == 0 ? 0 : c == 1 ? -sidemove : sidemove;
                sv_player->v.angles[0] = a;
                    
                char n[100];
                sprintf(n, "plot_lookupdown_ground_%s_look_%d.html",
                        c==0?"forward":c==1?"left":"right",
                        a);
                char n2[100];
                sprintf(n2, "Ground forward %s, look %d",
                        c==0?"":c==1?"Left":"Right", a);
                velocity[0] = velocity[1] = velocity[2] = 0;
                //stabilize();
                //if (!onground) SV_AirMove();
                if (!onground) stabilize();
                plot(n, n2);
            }
        }
    }
    if (false) {
        for (int a = -30; a <= 30; a+=10) {
            for (int c=0;c<3;c++) {
                onground = false;
                cmd.forwardmove = 0;
                float sidemove = 700;
                cmd.sidemove = c == 0 ? 0 : c == 1 ? -sidemove : sidemove;
                sv_player->v.angles[0] = a;
                    
                char n[100];
                sprintf(n, "plot_lookupdown_air_%s_look_%d.html",
                        c==0?"forward":c==1?"left":"right",
                        a);
                char n2[100];
                sprintf(n2, "Air %s, look %d",
                        c==0?"":c==1?"Left":"Right", a);
                velocity[0] = velocity[1] = velocity[2] = 0;
                //stabilize();
                //if (!onground) SV_AirMove();
                if (!onground) stabilize();
                plot(n, n2);
            }
        }
    }

    if (true) {
        plot_best_angle("plot_bestangle_power_bunny_left.html",
                        "Best viewing angle vs move direction for power bunny when strafing left", true);
        plot_best_angle("plot_bestangle_power_bunny_right.html",
                        "Best viewing angle vs move direction for power bunny when strafing right", false);
        plot_best_angle_speedadd("plot_bestangle_addspeed_power_bunny_left.html",
                                 "Best viewing angle speed change for power bunny when strafing left", true);
        plot_best_angle_speedadd("plot_bestangle_addspeed_power_bunny_right.html",
                                 "Best viewing angle speed change for power bunny when strafing right", false);
        /* plot_best_angle_and_addspeed("plot_bestangle_and_speed_power_bunny_right.html", */
        /*                 "Best viewing angle and speed for power bunny when strafing right"); */
        // best angle and speed increases for velocities
        onground = true;
        velocity[0] = velocity[1] = velocity[2] = 0;
        cmd.forwardmove = 400;
        cmd.sidemove = 700;
        for (int speed = 320; speed < 700; speed++) {
            velocity[1] = velocity[2] = 0;
            velocity[0] = speed;
            float oldspeed = sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);
            float bestangle = bestAngle();
            sv_player->v.angles[1] = bestangle;
            SV_AirMove();
            float newspeed = sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);
            printf("speed=%d bestangle=%.2f speedinc=%.2f\n", speed, bestangle, newspeed-oldspeed);
        }

        /*
montage \
plot_bestangle_power_bunny_left.png \
plot_bestangle_power_bunny_right.png \
-geometry +2-2 -tile 2x1 plot_bestangle_power_bunny.png

montage \
plot_bestangle_addspeed_power_bunny_left.png \
plot_bestangle_addspeed_power_bunny_right.png \
-geometry +2-2 -tile 2x1 plot_bestangle_addspeed_power_bunny.png
         */
    }

    
    /*
montage \
plot_ground_left_stand.png \
plot_ground_forward_stand.png \
plot_ground_right_stand.png \
plot_ground_left_walk.png \
plot_ground_forward_walk.png \
plot_ground_right_walk.png \
plot_ground_left_always_run_off.png \
plot_ground_forward_always_run_off.png \
plot_ground_right_always_run_off.png \
plot_ground_left_always_run_on.png \
plot_ground_forward_always_run_on.png \
plot_ground_right_always_run_on.png \
-geometry +2-2 -tile 3x4 plots_ground.png

montage \
plot_air_left_stand.png \
plot_air_forward_stand.png \
plot_air_right_stand.png \
plot_air_left_walk.png \
plot_air_forward_walk.png \
plot_air_right_walk.png \
plot_air_left_always_run_off.png \
plot_air_forward_always_run_off.png \
plot_air_right_always_run_off.png \
plot_air_left_always_run_on.png \
plot_air_forward_always_run_on.png \
plot_air_right_always_run_on.png \
-geometry +2-2 -tile 3x4 plots_air.png

montage \
plot_lookupdown_ground_left_look_-30.png \
plot_lookupdown_ground_forward_look_-30.png \
plot_lookupdown_ground_right_look_-30.png \
plot_lookupdown_ground_left_look_-20.png \
plot_lookupdown_ground_forward_look_-20.png \
plot_lookupdown_ground_right_look_-20.png \
plot_lookupdown_ground_left_look_-10.png \
plot_lookupdown_ground_forward_look_-10.png \
plot_lookupdown_ground_right_look_-10.png \
plot_lookupdown_ground_left_look_0.png \
plot_lookupdown_ground_forward_look_0.png \
plot_lookupdown_ground_right_look_0.png \
plot_lookupdown_ground_left_look_10.png \
plot_lookupdown_ground_forward_look_10.png \
plot_lookupdown_ground_right_look_10.png \
plot_lookupdown_ground_left_look_20.png \
plot_lookupdown_ground_forward_look_20.png \
plot_lookupdown_ground_right_look_20.png \
plot_lookupdown_ground_left_look_30.png \
plot_lookupdown_ground_forward_look_30.png \
plot_lookupdown_ground_right_look_30.png \
-geometry +2-2 -tile 3x7 plots_look_ground.png

montage \
plot_lookupdown_air_left_look_-30.png \
plot_lookupdown_air_forward_look_-30.png \
plot_lookupdown_air_right_look_-30.png \
plot_lookupdown_air_left_look_-20.png \
plot_lookupdown_air_forward_look_-20.png \
plot_lookupdown_air_right_look_-20.png \
plot_lookupdown_air_left_look_-10.png \
plot_lookupdown_air_forward_look_-10.png \
plot_lookupdown_air_right_look_-10.png \
plot_lookupdown_air_left_look_0.png \
plot_lookupdown_air_forward_look_0.png \
plot_lookupdown_air_right_look_0.png \
plot_lookupdown_air_left_look_10.png \
plot_lookupdown_air_forward_look_10.png \
plot_lookupdown_air_right_look_10.png \
plot_lookupdown_air_left_look_20.png \
plot_lookupdown_air_forward_look_20.png \
plot_lookupdown_air_right_look_20.png \
plot_lookupdown_air_left_look_30.png \
plot_lookupdown_air_forward_look_30.png \
plot_lookupdown_air_right_look_30.png \
-geometry +2-2 -tile 3x7 plots_look_air.png


         left, forward, right
stand
walk
arunoff
arunon
     */
    
    return 0;
}

