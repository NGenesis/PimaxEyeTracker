#include <windows.h>
#include <memory>
#include <string>
#include <list>
#include <algorithm>
#include <unordered_map>
#include <filesystem>
#include "PimaxEyeTracker.h"
#include "aSeeVRClient.h"
#include "aSeeVRTypes.h"
#include "aSeeVRUtility.h"
#include "resource.h"

class LoadDLL {
	public:
		LoadDLL() {
			_dll = LoadResourceDLL(IDR_ASEEVRCLIENT_DLL, (std::filesystem::temp_directory_path() / "aSeeVRClient.dll"));
		}

		~LoadDLL() {
			if(_dll) FreeLibrary(_dll);
		}

	private:
		HMODULE _dll;

		HMODULE LoadResourceDLL(WORD resourceID, std::filesystem::path filename) {
			HMODULE thisDLL = GetThisDLL();

			// Find and load the resource
			if(HRSRC resSource = FindResourceA(thisDLL, MAKEINTRESOURCEA(resourceID), "DLL")) {
				if(HGLOBAL resFile = LoadResource(thisDLL, resSource)) {
					// Write file to disk from resource
					DWORD bytesWritten = 0;
					if(HANDLE dllFile = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)) {
						if(WriteFile(dllFile, LockResource(resFile), SizeofResource(thisDLL, resSource), &bytesWritten, nullptr)) {
							CloseHandle(dllFile);
							return LoadLibraryW(filename.c_str());
						}
					}
				}
			}

			return nullptr;
		}

		static HMODULE GetThisDLL()
		{
			MEMORY_BASIC_INFORMATION info;
			return VirtualQueryEx(GetCurrentProcess(), (void*)GetThisDLL, &info, sizeof(info)) ? (HMODULE)info.AllocationBase : nullptr;
		}
} static g_LoadDLL;

struct EyeParameterState {
	float GazeX;
	float GazeY;
	float GazeRawX;
	float GazeRawY;
	float GazeSmoothX;
	float GazeSmoothY;
	float GazeOriginX;
	float GazeOriginY;
	float GazeOriginZ;
	float GazeDirectionX;
	float GazeDirectionY;
	float GazeDirectionZ;
	float GazeReliability;
	float PupilCenterX;
	float PupilCenterY;
	float PupilDistance;
	float PupilMajorDiameter;
	float PupilMajorUnitDiameter;
	float PupilMinorDiameter;
	float PupilMinorUnitDiameter;
	float Blink;
	float Openness;
	float UpperEyelid;
	float LowerEyelid;
	float PupilCenterSmoothX;
	float PupilCenterSmoothY;
	float OpennessSmooth;
};

struct EyeExpressionState {
	float PupilCenterX;
	float PupilCenterY;
	float Openness;
	bool Blink;
};

std::unordered_map<Eye, aSeeVREye> nativeEyeTypes = {
	{ Eye::Left, aSeeVREye::left_eye },
	{ Eye::Right, aSeeVREye::right_eye },
	{ Eye::Any, aSeeVREye::undefine_eye }
};

class EyeState {
	public:
		EyeState(Eye type) : Type(type), NativeType(nativeEyeTypes[type]), LastKnownPupilCenterX(0.0f), LastKnownPupilCenterY(0.0f) {}

		Eye Type;
		aSeeVREye NativeType;
		std::list<EyeParameterState> Samples;
		EyeExpressionState Expression;
		EyeParameterState Parameters;
		float LastKnownPupilCenterX;
		float LastKnownPupilCenterY;
};

std::unordered_map<Eye, std::shared_ptr<EyeState>> eyes = {
	{ Eye::Left, std::make_shared<EyeState>(Eye::Left) },
	{ Eye::Right, std::make_shared<EyeState>(Eye::Right) },
	{ Eye::Any, std::make_shared<EyeState>(Eye::Any) }
};

static const int MAX_SAMPLES = 20;
//std::list<EyeParameterState> left_eye_samples, right_eye_samples;
std::list<long long> sample_timestamps;
//EyeExpressionState left_eye_expression, right_eye_expression;
//EyeParameterState left_eye_param, right_eye_param, any_eye_param;
Eye eye_param_recommended = Eye::Any;
long long eye_param_timestamp = 0;
bool active = false;
aSeeVRCoefficient coefficient_data;

typedef int(_7INVENSUN_CALL *eyetracker_callback_t)();
eyetracker_callback_t start_callback = nullptr;
eyetracker_callback_t stop_callback = nullptr;
eyetracker_callback_t update_callback = nullptr;

void _7INVENSUN_CALL get_coefficient_callback(const aSeeVRCoefficient* data, void* context) {
	std::memcpy(coefficient_data.buf, data->buf, sizeof(data->buf));
}

void _7INVENSUN_CALL state_callback(const aSeeVRState* state, void* context) {
	switch(state->code) {
		case aSeeVRStateCode::api_start:
			active = true;
			if(start_callback) start_callback();
			break;
		case aSeeVRStateCode::api_stop: {
			bool wasActive = active;

			active = false;
			if(stop_callback && wasActive) {
				aSeeVR_disconnect_server();
				stop_callback();
			}
			break;
		}
		case aSeeVRStateCode::api_get_cofficient:
			break;
		default:
			break;
	}
}

void read_eye_data(Eye eye, const aSeeVREyeData* eye_data) {
	aSeeVRPoint2D pt2 = { 0 };
	aSeeVRPoint3D pt3 = { 0 };

	aSeeVREye native_eye = eyes[eye]->NativeType;
	EyeParameterState* target_eye_param = &eyes[eye]->Parameters;

	pt2 = { 0 };
	aSeeVR_get_point2d(eye_data, native_eye, aSeeVREyeDataItemType::gaze, &pt2);
	target_eye_param->GazeX = pt2.x;
	target_eye_param->GazeY = pt2.y;

	pt2 = { 0 };
	aSeeVR_get_point2d(eye_data, native_eye, aSeeVREyeDataItemType::gaze_raw, &pt2);
	target_eye_param->GazeRawX = pt2.x;
	target_eye_param->GazeRawY = pt2.y;

	pt2 = { 0 };
	aSeeVR_get_point2d(eye_data, native_eye, aSeeVREyeDataItemType::gaze_smooth, &pt2);
	target_eye_param->GazeSmoothX = pt2.x;
	target_eye_param->GazeSmoothY = pt2.y;

	pt3 = { 0 };
	aSeeVR_get_point3d(eye_data, native_eye, aSeeVREyeDataItemType::gaze_origin, &pt3);
	target_eye_param->GazeOriginX = pt3.x;
	target_eye_param->GazeOriginY = pt3.y;
	target_eye_param->GazeOriginZ = pt3.z;

	pt3 = { 0 };
	aSeeVR_get_point3d(eye_data, native_eye, aSeeVREyeDataItemType::gaze_direction, &pt3);
	target_eye_param->GazeDirectionX = pt3.x;
	target_eye_param->GazeDirectionY = pt3.y;
	target_eye_param->GazeDirectionZ = pt3.z;

	target_eye_param->GazeReliability = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::gaze_reliability, &(target_eye_param->GazeReliability));

	pt2 = { 0 };
	aSeeVR_get_point2d(eye_data, native_eye, aSeeVREyeDataItemType::pupil_center, &pt2);
	target_eye_param->PupilCenterX = pt2.x;
	target_eye_param->PupilCenterY = pt2.y;

	target_eye_param->PupilDistance = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::pupil_distance, &(target_eye_param->PupilDistance));

	target_eye_param->PupilMajorDiameter = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::pupil_diameter, &(target_eye_param->PupilMajorDiameter));

	target_eye_param->PupilMajorUnitDiameter = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::pupil_diameter_mm, &(target_eye_param->PupilMajorUnitDiameter));

	target_eye_param->PupilMinorDiameter = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::pupil_minoraxis, &(target_eye_param->PupilMinorDiameter));

	target_eye_param->PupilMinorUnitDiameter = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::pupil_minoraxis_mm, &(target_eye_param->PupilMinorUnitDiameter));

	int blink = 0;
	aSeeVR_get_int32(eye_data, native_eye, aSeeVREyeDataItemType::blink, &blink);
	target_eye_param->Blink = static_cast<float>(blink);

	target_eye_param->Openness = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::openness, &(target_eye_param->Openness));

	target_eye_param->UpperEyelid = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::upper_eyelid, &(target_eye_param->UpperEyelid));

	target_eye_param->LowerEyelid = 0.0f;
	aSeeVR_get_float(eye_data, native_eye, aSeeVREyeDataItemType::lower_eyelid, &(target_eye_param->LowerEyelid));
}

void update_sample_timestamps(long long timestamp) {
	sample_timestamps.push_front(timestamp);
	if(sample_timestamps.size() >= MAX_SAMPLES) sample_timestamps.pop_back();
}

void update_sample(Eye eye) {
	auto &eye_samples = eyes[eye]->Samples;
	eye_samples.push_front(eyes[eye]->Parameters);
	if(eye_samples.size() >= MAX_SAMPLES) eye_samples.pop_back();
}

void update_expression(Eye eye) {
	auto eye_state = eyes[eye];
	//auto other_eye_state = eyes[eye == Eye::Left ? Eye::Right : Eye::Left];
	auto &eye_samples = eye_state->Samples;
	auto *eye_expression = &eye_state->Expression;
	/*eye_expression->PupilCenterX = std::clamp((eye_param->PupilCenterX <= std::numeric_limits<float>::epsilon()) ? 0.0f : (eye_param->PupilCenterX * 2.0f - 1.0f) * 5.0f, -1.0f, 1.0f);
	eye_expression->PupilCenterY = -std::clamp((eye_param->PupilCenterY <= std::numeric_limits<float>::epsilon()) ? 0.0f : (eye_param->PupilCenterY * 2.0f - 1.0f) * 5.0f + 2.0f, -1.0f, 1.0f);
	eye_expression->Openness = (100.0f - eye_param->Openness) / 100.0f;
	eye_expression->Blink = (eye_param->Openness <= 0.2f);*/
	//eye_expression->Blink = (eye_param->PupilCenterX <= std::numeric_limits<float>::epsilon() && eye_param->PupilCenterY <= std::numeric_limits<float>::epsilon());
	
	// Update eye lid states from accumulated samples
	// TODO: Keep eyes open when tracking is out of range
	float blink = 0.0f;
	static const size_t MAX_BLINK_SAMPLES = 7;
	size_t blinkSamples = 0;
	for(const auto &sample : eye_samples) {
		if(blinkSamples++ >= MAX_BLINK_SAMPLES) break;
		blink += (sample.PupilCenterX <= std::numeric_limits<float>::epsilon() && sample.PupilCenterY <= std::numeric_limits<float>::epsilon()) ? 1.0f : 0.0f;
	}
	if(!eye_samples.empty()) blink /= (std::min)(MAX_BLINK_SAMPLES, eye_samples.size());
	eye_expression->Blink = (blink >= 0.7f);
	eye_expression->Openness = eye_expression->Blink ? 0.0f : 1.0f;

	// Update eye movement from accumulated samples
	// TODO: Predict eye position when blinking or tracking is lost
	eye_expression->PupilCenterX = eye_expression->PupilCenterY = 0.0f;
	static const size_t MAX_EYE_SAMPLES = 5;
	size_t eyeSamples = 0;
	for(const auto &sample : eye_samples) {
		if(eyeSamples++ >= MAX_EYE_SAMPLES) break;
		eye_expression->PupilCenterX += std::clamp((sample.PupilCenterX <= std::numeric_limits<float>::epsilon()) ? 0.0f : (sample.PupilCenterX * 2.0f - 1.0f) * 5.0f, -1.0f, 1.0f);
		eye_expression->PupilCenterY += -std::clamp((sample.PupilCenterY <= std::numeric_limits<float>::epsilon()) ? 0.0f : (sample.PupilCenterY * 2.0f - 1.0f) * 5.0f + 2.0f, -1.0f, 1.0f);
	}
	if(!eye_samples.empty()) {
		eye_expression->PupilCenterX /= (std::min)(MAX_EYE_SAMPLES, eye_samples.size());
		eye_expression->PupilCenterY /= (std::min)(MAX_EYE_SAMPLES, eye_samples.size());
	}

	// Use last known position when tracking is lost
	if(eye_expression->PupilCenterX == 0.0f && eye_expression->PupilCenterY == 0.0f) {
		eye_expression->PupilCenterX = eye_state->LastKnownPupilCenterX;
		eye_expression->PupilCenterY = eye_state->LastKnownPupilCenterY;

		eye_expression->Blink = 1.0f;
		eye_expression->Openness = eye_expression->Blink ? 0.0f : 1.0f;
	}

	// Update last known position
	if(eye_expression->PupilCenterX != 0.0f || eye_expression->PupilCenterY != 0.0f) {
		eye_state->LastKnownPupilCenterX = eye_expression->PupilCenterX;
		eye_state->LastKnownPupilCenterY = eye_expression->PupilCenterY;
	}
}

void _7INVENSUN_CALL eye_data_callback(const aSeeVREyeData* eye_data, void* context) {
	if(!eye_data) return;

	aSeeVR_get_int64(eye_data, aSeeVREye::undefine_eye, aSeeVREyeDataItemType::timestamp, &eye_param_timestamp);
	update_sample_timestamps(eye_param_timestamp);

	int recommend = -1;
	aSeeVR_get_int32(eye_data, aSeeVREye::undefine_eye, aSeeVREyeDataItemType::recommend, &recommend);
	eye_param_recommended = (recommend == 1) ? Eye::Left : Eye::Right; // 1 (left) or 2 (right)

	read_eye_data(Eye::Any, eye_data);

	read_eye_data(Eye::Left, eye_data);
	update_sample(Eye::Left);

	read_eye_data(Eye::Right, eye_data);
	update_sample(Eye::Right);

	update_expression(Eye::Left);
	update_expression(Eye::Right);

	if(update_callback) update_callback();
}

extern "C" {
	void RegisterCallback(CallbackType type, eyetracker_callback_t callback) {
		switch(type) {
			case CallbackType::Start:
				start_callback = callback;
				break;

			case CallbackType::Stop:
				stop_callback = callback;
				break;

			case CallbackType::Update:
				update_callback = callback;
				break;
		}
	}

	bool Start() {
		if(active) return true;

		aSeeVRInitParam param;
		param.ports[0] = 5777; // Runtime Port
		if(aSeeVR_connect_server(&param) != aSeeVRReturnCode::success) return false;

		if(aSeeVR_register_callback(aSeeVRCallbackType::state, state_callback, nullptr) != aSeeVRReturnCode::success || aSeeVR_register_callback(aSeeVRCallbackType::coefficient, get_coefficient_callback, nullptr) != aSeeVRReturnCode::success || aSeeVR_register_callback(aSeeVRCallbackType::eye_data, eye_data_callback, nullptr) != aSeeVRReturnCode::success) {
			aSeeVR_disconnect_server();
			return false;
		}

		if(aSeeVR_get_coefficient() != aSeeVRReturnCode::success) {
			aSeeVR_disconnect_server();
			return false;
		}

		if(aSeeVR_start(&coefficient_data) != aSeeVRReturnCode::success) {
			aSeeVR_disconnect_server();
			return false;
		}

		return true;
	}

	void Stop() {
		if(!active) return;

		active = false;
		aSeeVR_stop();
		aSeeVR_disconnect_server();
		if(stop_callback) stop_callback();
	}

	float GetEyeParameter(Eye eye, EyeParameter param) {
		EyeParameterState* eye_param = &eyes[eye]->Parameters;

		switch(param) {
			case EyeParameter::GazeX: return eye_param->GazeX;
			case EyeParameter::GazeY: return eye_param->GazeY;
			case EyeParameter::GazeRawX: return eye_param->GazeRawX;
			case EyeParameter::GazeRawY: return eye_param->GazeRawY;
			case EyeParameter::GazeSmoothX: return eye_param->GazeSmoothX;
			case EyeParameter::GazeSmoothY: return eye_param->GazeSmoothY;
			case EyeParameter::GazeOriginX: return eye_param->GazeOriginX;
			case EyeParameter::GazeOriginY: return eye_param->GazeOriginY;
			case EyeParameter::GazeOriginZ: return eye_param->GazeOriginZ;
			case EyeParameter::GazeDirectionX: return eye_param->GazeDirectionX;
			case EyeParameter::GazeDirectionY: return eye_param->GazeDirectionY;
			case EyeParameter::GazeDirectionZ: return eye_param->GazeDirectionZ;
			case EyeParameter::GazeReliability: return eye_param->GazeReliability;
			case EyeParameter::PupilCenterX: return eye_param->PupilCenterX;
			case EyeParameter::PupilCenterY: return eye_param->PupilCenterY;
			case EyeParameter::PupilDistance: return eye_param->PupilDistance;
			case EyeParameter::PupilMajorDiameter: return eye_param->PupilMajorDiameter;
			case EyeParameter::PupilMajorUnitDiameter: return eye_param->PupilMajorUnitDiameter;
			case EyeParameter::PupilMinorDiameter: return eye_param->PupilMinorDiameter;
			case EyeParameter::PupilMinorUnitDiameter: return eye_param->PupilMinorUnitDiameter;
			case EyeParameter::Blink: return eye_param->Blink;
			case EyeParameter::Openness: return eye_param->Openness;
			case EyeParameter::UpperEyelid: return eye_param->UpperEyelid;
			case EyeParameter::LowerEyelid: return eye_param->LowerEyelid;
		}

		return 0.0f;
	}

	float GetEyeExpression(Eye eye, EyeExpression expression) {
		EyeExpressionState* eye_expression = &eyes[eye]->Expression;

		switch(expression) {
			case EyeExpression::PupilCenterX: return eye_expression->PupilCenterX;
			case EyeExpression::PupilCenterY: return eye_expression->PupilCenterY;
			case EyeExpression::Blink: return eye_expression->Blink ? 1.0f : 0.0f;
			case EyeExpression::Openness: return eye_expression->Openness;
		}

		return 0.0f;
	}

	long long GetTimestamp() {
		return eye_param_timestamp;
	}

	Eye GetRecommendedEye() {
		return eye_param_recommended;
	}

	bool IsActive() {
		return active;
	}
}