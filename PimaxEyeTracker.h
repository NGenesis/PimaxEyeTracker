#pragma once

#define PIMAXEYETRACKER_API __declspec(dllexport)

enum class EyeParameter {
	GazeX, // Gaze point on the X axis (not working!)
	GazeY, // Gaze point on then Y axis (not working!)
	GazeRawX, // Gaze point on the X axis before smoothing is applied (not working!)
	GazeRawY, // Gaze point on the Y axis before smoothing is applied (not working!)
	GazeSmoothX, // Gaze point on the X axis after smoothing is applied (not working!)
	GazeSmoothY, // Gaze point on the Y axis after smoothing is applied (not working!)
	GazeOriginX, // Pupil gaze origin on the X axis
	GazeOriginY, // Pupil gaze origin on the Y axis
	GazeOriginZ, // Pupil gaze origin on the Z axis
	GazeDirectionX, // Gaze vector on the X axis (not working!)
	GazeDirectionY, // Gaze vector on the Y axis (not working!)
	GazeDirectionZ, // Gaze vector on the Z axis (not working!)
	GazeReliability, // Gaze point reliability (not working!)
	PupilCenterX, // Pupil center on the X axis, normalized between 0 and 1
	PupilCenterY, // Pupil center on the Y axis, normalized between 0 and 1
	PupilDistance, // Distance between pupil and camera lens, measured in millimeters
	PupilMajorDiameter, // Pupil major axis diameter, normalized between 0 and 1
	PupilMajorUnitDiameter, // Pupil major axis diameter, measured in millimeters
	PupilMinorDiameter, // Pupil minor axis diameter, normalized between 0 and 1
	PupilMinorUnitDiameter, // Pupil minor axis diameter, measured in millimeters
	Blink, // Blink state (not working!)
	Openness, // How open the eye is - 100 (closed), 50 (partially open, unreliable), 0 (open)
	UpperEyelid, // Upper eyelid state (not working!)
	LowerEyelid // Lower eyelid state (not working!)
};

enum class EyeExpression {
	PupilCenterX, // Pupil center on the X axis, smoothed and normalized between -1 (looking left) ... 0 (looking forward) ... 1 (looking right)
	PupilCenterY, // Pupil center on the Y axis, smoothed and normalized between -1 (looking down) ... 0 (looking forward) ... 1 (looking up)
	Openness, // How open the eye is, smoothed and normalized between 0 (fully closed) ... 1 (fully open)
	Blink // Blink, 0 (not blinking) or 1 (blinking)
};

enum class Eye {
	Any,
	Left,
	Right
};

enum class CallbackType {
	Start,
	Stop,
	Update
};

typedef int(__stdcall *eyetracker_callback_t)();

extern "C" {
	PIMAXEYETRACKER_API void RegisterCallback(CallbackType type, eyetracker_callback_t callback);

	PIMAXEYETRACKER_API bool Start();
	PIMAXEYETRACKER_API void Stop();
	PIMAXEYETRACKER_API bool IsActive();

	PIMAXEYETRACKER_API long long GetTimestamp();
	PIMAXEYETRACKER_API Eye GetRecommendedEye();
	PIMAXEYETRACKER_API float GetEyeParameter(Eye eye, EyeParameter param);
	PIMAXEYETRACKER_API float GetEyeExpression(Eye eye, EyeExpression expression);
}