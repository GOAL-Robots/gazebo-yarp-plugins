/*
 * Copyright (C) 2013-2015 Fondazione Istituto Italiano di Tecnologia RBCS & iCub Facility & ADVR
 * Authors: see AUTHORS file.
 * CopyPolicy: Released under the terms of the LGPLv2.1 or any later version, see LGPL.TXT or LGPL3.TXT
 */


#include "gazebo/MultiCamera.hh"
#include "yarp/dev/MultiCameraDriver.h"

#include <GazeboYarpPlugins/Handler.hh>
#include <GazeboYarpPlugins/common.h>
#include <GazeboYarpPlugins/ConfHelpers.hh>

#include <gazebo/sensors/MultiCameraSensor.hh>
#include <gazebo/sensors/DepthCameraSensor.hh>
#include <gazebo/sensors/CameraSensor.hh>

#include <yarp/os/Network.h>
#include <yarp/os/LogStream.h>
#include <yarp/dev/PolyDriver.h>


GZ_REGISTER_SENSOR_PLUGIN(gazebo::GazeboYarpMultiCamera)


namespace gazebo {

GazeboYarpMultiCamera::GazeboYarpMultiCamera() :
        MultiCameraPlugin()
{
}

GazeboYarpMultiCamera::~GazeboYarpMultiCamera()
{
    this->m_cameraDriver.close();
    GazeboYarpPlugins::Handler::getHandler()->removeSensor(this->m_sensorName);
}

void GazeboYarpMultiCamera::Load(sensors::SensorPtr _sensor, sdf::ElementPtr _sdf)
{
    if (!yarp::os::NetworkBase::checkNetwork(GazeboYarpPlugins::yarpNetworkInitializationTimeout)) {
        yError() << "GazeboYarpMultiCamera::Load error: yarp network does not seem to be available, is the yarpserver running?";
        return;
    }

    MultiCameraPlugin::Load(_sensor, _sdf);

    m_sensor = parentSensor.get();
    yAssert(m_sensor != NULL);

    // Add my gazebo device driver to the factory.
    yarp::dev::Drivers::factory().add(new ::yarp::dev::DriverCreatorOf< ::yarp::dev::GazeboYarpMultiCameraDriver>
                                      ("gazebo_multicamera", "grabber", "GazeboYarpMultiCameraDriver"));

    //Getting .ini configuration file from sdf
    bool configuration_loaded = GazeboYarpPlugins::loadConfigSensorPlugin(_sensor, _sdf, m_parameters);

    if (!configuration_loaded) {
        yWarning() << "MultiCameraPlugin could not load configuration";
        return;
    }

#if GAZEBO_MAJOR_VERSION >= 7
    m_sensorName = _sensor->ScopedName();
#else
    m_sensorName = _sensor->GetScopedName();
#endif

    yDebug() << "GazeboYarpMultiCamera Plugin: sensor scoped name is" << m_sensorName.c_str();
    //Insert the pointer in the singleton handler for retriving it in the yarp driver
    GazeboYarpPlugins::Handler::getHandler()->setSensor(_sensor.get());

    m_parameters.put(YarpScopedName.c_str(), m_sensorName.c_str());

    //Open the driver
    if (m_cameraDriver.open(m_parameters)) {
        yInfo() << "Loaded GazeboYarpMultiCamera Plugin correctly";
    } else {
        yWarning() << "GazeboYarpMultiCamera Plugin Load failed: error in opening yarp driver";
    }

    m_cameraDriver.view(iFrameGrabberImage);
    if(iFrameGrabberImage == NULL) {
        yError() << "Unable to get the iFrameGrabberImage interface from the device";
        return;
    }
}

} // namespace gazebo
