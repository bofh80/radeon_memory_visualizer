//=============================================================================
/// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Model header for the Device configuration pane
//=============================================================================

#ifndef RMV_MODELS_TIMELINE_DEVICE_CONFIGURATION_MODEL_H_
#define RMV_MODELS_TIMELINE_DEVICE_CONFIGURATION_MODEL_H_

#include "qt_common/utils/model_view_mapper.h"

namespace rmv
{
    /// An enum of widgets used by the UI and model. Used to map UI widgets to their
    /// corresponding model data.
    enum DeviceConfigurationWidgets
    {
        kDeviceConfigurationDeviceName,
        kDeviceConfigurationDeviceID,
        kDeviceConfigurationMemorySize,
        kDeviceConfigurationShaderCoreClockFrequency,
        kDeviceConfigurationMemoryClockFrequency,
        kDeviceConfigurationLocalMemoryBandwidth,
        kDeviceConfigurationLocalMemoryType,
        kDeviceConfigurationLocalMemoryBusWidth,

        kDeviceConfigurationNumWidgets
    };

    /// Container class that holds model data for a given pane.
    class DeviceConfigurationModel : public ModelViewMapper
    {
    public:
        /// Constructor.
        explicit DeviceConfigurationModel();

        /// Destructor.
        virtual ~DeviceConfigurationModel();

        /// Initialize blank data for the model.
        void ResetModelValues();

        /// Update the model with data from the back end.
        void Update();
    };

}  // namespace rmv

#endif  // RMV_MODELS_TIMELINE_DEVICE_CONFIGURATION_MODEL_H_
