/*
 * Copyright (C) 2007-2013 Istituto Italiano di Tecnologia ADVR & iCub Facility
 * Authors: Enrico Mingo, Alessio Rocchi, Mirko Ferrati, Silvio Traversaro and Alessandro Settimi
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 */

#include "gazebo_yarp_plugins/Clock.hh"
#include <gazebo/physics/physics.hh>
#include <yarp/os/Property.h>
#include <iostream>
#include <cmath>

namespace gazebo
{

    GazeboYarpClock::GazeboYarpClock() : _yarp()
    {

    }

    GazeboYarpClock::~GazeboYarpClock()
    {
        gazebo::event::Events::DisconnectWorldUpdateBegin(time_update_event_);
    }


    void GazeboYarpClock::Load(int _argc, char **_argv)
    {
        if( !_yarp.checkNetwork() ) {
            std::cerr << "GazeboYarpClock::Load error: yarp network does not seem to be available, is the yarpserver running?"<<std::endl;
            return;
        }

        std::cout << "GazeboYarpClock loaded." << std::endl;

        port_name = "/gazebo_yarp_clock:o";

        topic_name = "/clock";

        //The proper loading is done when the world is created
        load_gazebo_yarp_clock = gazebo::event::Events::ConnectWorldCreated(boost::bind(&GazeboYarpClock::GazeboYarpClockLoad,this,_1));
    }

    void GazeboYarpClock::GazeboYarpClockLoad(std::string world_name)
    {
          gazebo::event::Events::DisconnectWorldCreated(load_gazebo_yarp_clock);

          //Opening port
          port.open(port_name);

          //Connecting port to topic (and creating topic)
          _yarp.connect(port_name,"topic:/"+topic_name);

          //Getting world pointer
          world_ = gazebo::physics::get_world(world_name);

          time_update_event_ = gazebo::event::Events::ConnectWorldUpdateBegin(boost::bind(&GazeboYarpClock::ClockUpdate,this));
    }

    void GazeboYarpClock::ClockUpdate()
    {
         gazebo::common::Time currentTime = world_->GetSimTime();
         yarp::os::Bottle& b = port.prepare();
         b.clear();
         b.addInt(currentTime.sec);
         b.addInt(currentTime.nsec);
         port.write();
    }

    // Register this plugin with the simulator
    GZ_REGISTER_SYSTEM_PLUGIN(GazeboYarpClock)
}
