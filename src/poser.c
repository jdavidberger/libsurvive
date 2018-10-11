#include "math.h"
#include <assert.h>
#include <linmath.h>
#include <stdint.h>
#include <stdio.h>
#include <survive.h>

#define _USE_MATH_DEFINES // for C
#include <math.h>
#include <poser.h>
#include <string.h>

static uint32_t PoserData_timecode(PoserData *poser_data) {
	switch (poser_data->pt) {
	case POSERDATA_LIGHT: {
		PoserDataLight *lightData = (PoserDataLight *)poser_data;
		return lightData->timecode;
	}
	case POSERDATA_FULL_SCENE: {
		PoserDataFullScene *pdfs = (PoserDataFullScene *)(poser_data);
		return -1;
	}
	case POSERDATA_IMU: {
		PoserDataIMU *imuData = (PoserDataIMU *)poser_data;
		return imuData->timecode;
	}
	}
	return -1;
}

STATIC_CONFIG_ITEM(REPORT_IN_IMU, "report-in-imu", 'i', "Debug option to output poses in IMU space.", 0);
void PoserData_poser_pose_func(PoserData *poser_data, SurviveObject *so, const SurvivePose *imu2world) {
	if (poser_data->poseproc) {
		poser_data->poseproc(so, PoserData_timecode(poser_data), imu2world, poser_data->userdata);
	} else {
		static int report_in_imu = -1;
		if (report_in_imu == -1) {
			survive_attach_configi(so->ctx, "report-in-imu", &report_in_imu);
		}

		SurviveContext *ctx = so->ctx;
		SurvivePose head2world;
		so->OutPoseIMU = *imu2world;
		if (!report_in_imu) {
			ApplyPoseToPose(&head2world, imu2world, &so->head2imu);
		} else {
			head2world = *imu2world;
		}

		for (int i = 0; i < 7; i++)
			assert(!isnan(((double *)imu2world)[i]));

		so->ctx->poseproc(so, PoserData_timecode(poser_data), &head2world);
	}
}
void PoserData_poser_pose_func_with_velocity(PoserData *poser_data, SurviveObject *so, const SurvivePose *imu2world,
											 const SurviveVelocity *velocity) {
	PoserData_poser_pose_func(poser_data, so, imu2world);
	so->ctx->velocityproc(so, PoserData_timecode(poser_data), velocity);
}

void PoserData_lighthouse_pose_func(PoserData *poser_data, SurviveObject *so, uint8_t lighthouse,
									SurvivePose *arb2world, SurvivePose *lighthouse_pose, SurvivePose *object_pose) {
	if (poser_data->lighthouseposeproc) {
		poser_data->lighthouseposeproc(so, lighthouse, lighthouse_pose, object_pose, poser_data->userdata);
	} else {
		const FLT up[3] = {0, 0, 1};

		if (quatmagnitude(lighthouse_pose->Rot) == 0) {
			SurviveContext *ctx = so->ctx;
			SV_INFO("Pose func called with invalid pose.");
			return;
		}

		// Assume that the space solved for is valid but completely arbitrary. We are going to do a few things:
		// a) Using the gyro data, normalize it so that gravity is pushing straight down along Z
		// c) Assume the object is at origin
		// b) Place the first lighthouse on the X axis by rotating around Z
		//
		// This calibration setup has the benefit that as long as the user is calibrating on the same flat surface,
		// calibration results will be roughly identical between all posers no matter the orientation the object is
		// lying
		// in.
		//
		// We might want to go a step further and affix the first lighthouse in a given pose that preserves up so that
		// it doesn't matter where on that surface the object is.
		bool worldEstablished = quatmagnitude(arb2world->Rot) != 0;
		SurvivePose object2arb = {.Rot = {1.}};
		if (object_pose)
			object2arb = *object_pose;
		SurvivePose lighthouse2arb = *lighthouse_pose;

		SurvivePose obj2world, lighthouse2world;
		// Purposefully only set this once. It should only depend on the first (calculated) lighthouse
		if (!worldEstablished) {

			// Start by just moving from whatever arbitrary space into object space.
			SurvivePose arb2object;
			InvertPose(&arb2object, &object2arb);

			SurvivePose lighthouse2obj;
			ApplyPoseToPose(&lighthouse2obj, &arb2object, &lighthouse2arb);
			*arb2world = arb2object;

			// Find the poses that map to the above

			// Find the space with the same origin, but rotated so that gravity is up
			SurvivePose lighthouse2objUp = {0}, object2objUp = {0};
			if (quatmagnitude(so->activations.accel)) {
				quatfrom2vectors(object2objUp.Rot, so->activations.accel, up);
			} else {
				object2objUp.Rot[0] = 1.0;
			}

			// Calculate the pose of the lighthouse in this space
			ApplyPoseToPose(&lighthouse2objUp, &object2objUp, &lighthouse2obj);
			ApplyPoseToPose(arb2world, &object2objUp, arb2world);

			// Find what angle we need to rotate about Z by to get to 90 degrees.
			FLT ang = atan2(lighthouse2objUp.Pos[1], lighthouse2objUp.Pos[0]);
			FLT euler[3] = {0, 0, M_PI / 2. - ang};
			SurvivePose objUp2World = { 0 };
			quatfromeuler(objUp2World.Rot, euler);

			ApplyPoseToPose(arb2world, &objUp2World, arb2world);
		}

		ApplyPoseToPose(&obj2world, arb2world, &object2arb);
		ApplyPoseToPose(&lighthouse2world, arb2world, &lighthouse2arb);

		so->ctx->lighthouseposeproc(so->ctx, lighthouse, &lighthouse2world, &obj2world);
	}
}

void PoserDataFullScene2Activations(const PoserDataFullScene *pdfs, SurviveSensorActivations *activations) {
	memset(activations, 0, sizeof(SurviveSensorActivations));

	for (int i = 0; i < SENSORS_PER_OBJECT * NUM_LIGHTHOUSES * 2; i++) {
		double length = ((double *)pdfs->lengths)[i] * 48000000;
		if (length > 0)
			((survive_timecode *)activations->lengths)[i] = (survive_timecode)length;
		((double *)activations->angles)[i] = ((double *)pdfs->angles)[i];
	}
	memcpy(activations->accel, pdfs->lastimu.accel, sizeof(activations->accel));
	memcpy(activations->gyro, pdfs->lastimu.gyro, sizeof(activations->gyro));
	memcpy(activations->mag, pdfs->lastimu.mag, sizeof(activations->mag));
}
