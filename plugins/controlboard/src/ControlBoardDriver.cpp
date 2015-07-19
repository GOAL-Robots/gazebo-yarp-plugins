/*
 * Copyright (C) 2007-2013 Istituto Italiano di Tecnologia ADVR & iCub Facility
 * Authors: Enrico Mingo, Alessio Rocchi, Mirko Ferrati, Silvio Traversaro and Alessandro Settimi
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 */

#include <map>

#include "ControlBoardDriver.h"
#include <GazeboYarpPlugins/common.h>

#include <cstdio>
#include <gazebo/physics/physics.hh>
#include <gazebo/common/common.hh>
#include <gazebo/transport/transport.hh>
#include <gazebo/math/Angle.hh>

#include <yarp/os/LogStream.h>

using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::dev;


const double RobotPositionTolerance = 0.9;
gazebo::common::Time previousGazeboSimTime;

GazeboYarpControlBoardDriver::GazeboYarpControlBoardDriver() : deviceName("") {}
GazeboYarpControlBoardDriver::~GazeboYarpControlBoardDriver() {}

bool GazeboYarpControlBoardDriver::gazebo_init()
{
    //m_robot = gazebo_pointer_wrapper::getModel();
    // std::cout<<"if this message is the last one you read, m_robot has not been set"<<std::endl;
    //assert is a NOP in release mode. We should change the error handling either with an exception or something else
    assert(m_robot);
    if (!m_robot) return false;

    std::cout<<"Robot Name: "<<m_robot->GetName() <<std::endl;
    std::cout<<"# Joints: "<<m_robot->GetJoints().size() <<std::endl;
    std::cout<<"# Links: "<<m_robot->GetLinks().size() <<std::endl;

<<<<<<< HEAD
    this->m_robotRefreshPeriod = (unsigned)(this->m_robot->GetWorld()->GetPhysicsEngine()->GetUpdatePeriod() * 1000.0);
=======
    this->m_robotRefreshPeriod = 0.01 * 1000.0;
>>>>>>> back to update info time step
    if (!setJointNames()) return false;

    m_numberOfJoints = m_jointNames.size();
    m_positions.resize(m_numberOfJoints);
    m_zeroPosition.resize(m_numberOfJoints);
    m_referenceVelocities.resize(m_numberOfJoints);
    m_velocities.resize(m_numberOfJoints);
    amp.resize(m_numberOfJoints);
    m_torques.resize(m_numberOfJoints); m_torques.zero();
    m_trajectoryGenerationReferenceSpeed.resize(m_numberOfJoints);
    m_referencePositions.resize(m_numberOfJoints);
    m_trajectoryGenerationReferencePosition.resize(m_numberOfJoints);
    m_trajectoryGenerationReferenceAcceleraton.resize(m_numberOfJoints);
    m_referenceTorques.resize(m_numberOfJoints);
    m_jointLimits.resize(m_numberOfJoints);
    m_positionPIDs.reserve(m_numberOfJoints);
    m_velocityPIDs.reserve(m_numberOfJoints);
    m_impedancePosPDs.reserve(m_numberOfJoints);
    m_torqueOffsett.resize(m_numberOfJoints);
    m_minStiffness.resize(m_numberOfJoints, 0.0);
    m_maxStiffness.resize(m_numberOfJoints, 1000.0);
    m_minDamping.resize(m_numberOfJoints, 0.0);
    m_maxDamping.resize(m_numberOfJoints, 100.0);

    // m_gazeboPositionPIDs.resize(m_numberOfJoints);
    // m_gazeboVelocityPIDs.resize(m_numberOfJoints);
    // m_gazeboImpedancePosPIDs.resize(m_numberOfJoints);

    setMinMaxPos();
    setMinMaxImpedance();
    setPIDs();
    m_positions.zero();
    m_zeroPosition.zero();
    m_referenceVelocities.zero();
    m_velocities.zero();
    m_trajectoryGenerationReferenceSpeed.zero();
    m_referencePositions.zero();
    m_trajectoryGenerationReferencePosition.zero();
    m_trajectoryGenerationReferenceAcceleraton.zero();
    m_referenceTorques.zero();
    amp = 1; // initially on - ok for simulator
    started = false;
    m_controlMode = new int[m_numberOfJoints];
    m_interactionMode = new int[m_numberOfJoints];
    m_isMotionDone = new bool[m_numberOfJoints];
    m_clock = 0;
    m_torqueOffsett = 0;
    for (unsigned int j = 0; j < m_numberOfJoints; ++j)
        m_controlMode[j] = VOCAB_CM_POSITION;
    for (unsigned int j = 0; j < m_numberOfJoints; ++j)
        m_interactionMode[j] = VOCAB_IM_STIFF;

    std::cout << "gazebo_init set pid done!" << std::endl;

    this->m_updateConnection
        = gazebo::event::Events::ConnectWorldUpdateBegin(boost::bind(&GazeboYarpControlBoardDriver::onUpdate,
                                                                     this, _1));

    m_gazeboNode = gazebo::transport::NodePtr(new gazebo::transport::Node);
    m_gazeboNode->Init(this->m_robot->GetWorld()->GetName());
    m_jointCommandPublisher = m_gazeboNode->Advertise<gazebo::msgs::JointCmd>(std::string("~/") + this->m_robot->GetName() + "/joint_cmd");

    _T_controller = 1;

    std::stringstream ss(m_pluginParameters.find("initialConfiguration").toString());
    if (!(m_pluginParameters.find("initialConfiguration") == "")) {
        double tmp = 0.0;
        yarp::sig::Vector initial_config(m_numberOfJoints);
        unsigned int counter = 1;
        while (ss >> tmp) {
            if(counter > m_numberOfJoints) {
                std::cout<<"To many element in initial configuration, stopping at element "<<counter<<std::endl;
                break;
            }
            initial_config[counter-1] = tmp;
            m_trajectoryGenerationReferencePosition[counter - 1] = GazeboYarpPlugins::convertRadiansToDegrees(tmp);
            m_referencePositions[counter - 1] = GazeboYarpPlugins::convertRadiansToDegrees(tmp);
            m_positions[counter - 1] = GazeboYarpPlugins::convertRadiansToDegrees(tmp);
            counter++;
        }
        std::cout<<"INITIAL CONFIGURATION IS: "<<initial_config.toString()<<std::endl;

        for (unsigned int i = 0; i < m_numberOfJoints; ++i) {
#if GAZEBO_MAJOR_VERSION >= 4
            m_jointPointers[i]->SetPosition(0,initial_config[i]);
#else
            gazebo::math::Angle a;
            a.SetFromRadian(initial_config[i]);
            m_jointPointers[i]->SetAngle(0,a);
#endif
        }
    }
    return true;
}

void GazeboYarpControlBoardDriver::computeTrajectory(const int j)
{
    if ((m_referencePositions[j] - m_trajectoryGenerationReferencePosition[j]) < -RobotPositionTolerance) {
        if (m_trajectoryGenerationReferenceSpeed[j] !=0)
            m_referencePositions[j] += (m_trajectoryGenerationReferenceSpeed[j] / 1000.0) * m_robotRefreshPeriod * _T_controller;
        m_isMotionDone[j] = false;
    } else if ((m_referencePositions[j] - m_trajectoryGenerationReferencePosition[j]) > RobotPositionTolerance) {
        if (m_trajectoryGenerationReferenceSpeed[j] != 0)
            m_referencePositions[j]-= (m_trajectoryGenerationReferenceSpeed[j] / 1000.0) * m_robotRefreshPeriod * _T_controller;
        m_isMotionDone[j] = false;
    } else {
        m_referencePositions[j] = m_trajectoryGenerationReferencePosition[j];
        m_isMotionDone[j] = true;
    }
}

void GazeboYarpControlBoardDriver::onUpdate(const gazebo::common::UpdateInfo& _info)
{
    m_clock++;

    if (!started) {//This is a simple way to start with the robot in standing position
        started = true;
        std::cout<<"\n\nRefresh period: "<<m_robot->GetWorld()->GetPhysicsEngine()->GetUpdatePeriod() <<"\n\n"<<std::endl;
        for (unsigned int j = 0; j < m_numberOfJoints; ++j) {
            sendPositionToGazebo (j, m_positions[j]);
            //double diff = m_jointPointers[j]->GetAngle(0).Radian() - GazeboYarpPlugins::convertDegreesToRadians(m_referencePositions[j]);
            //double cmd = m_gazeboPositionPIDs[j].Update(diff, 0.01);
            //m_jointPointers[j]->SetForce(0, cmd);
            //std::cout << "Joint number: " << j << " P: " << m_gazeboPositionPIDs[m_jointNames[j]].GetPGain() << " I: " << m_gazeboPositionPIDs[m_jointNames[j]].GetIGain() << " D: " << m_gazeboPositionPIDs[m_jointNames[j]].GetDGain() << std::endl;
        }
        //m_jointController = m_robot->GetJointController();
        //m_gazeboPositionPIDs = this->m_jointController->GetPositionPIDs();
        for (unsigned int j = 0; j < m_numberOfJoints; ++j) {
            //sendPositionToGazebo (j, m_positions[j]);
            //m_gazeboPositionPIDs[m_jointNames[j]].SetPGain(1000);
            std::cout << "Joint number: " << j << " P: " << m_gazeboPositionPIDs[j].GetPGain() << " I: " << m_gazeboPositionPIDs[j].GetIGain() << " D: " << m_gazeboPositionPIDs[j].GetDGain() << std::endl;
        }
        previousGazeboSimTime = m_robot->GetWorld()->GetSimTime();
        //std::cout << "P: " << m_gazeboPositionPIDs[m_jointNames[0]].GetPGain() << "I: " << m_gazeboPositionPIDs[m_jointNames[0]].GetIGain() << "D: " << m_gazeboPositionPIDs[m_jointNames[0]].GetDGain() << std::endl;
    }

    // Sensing position & torque
    for (unsigned int jnt_cnt = 0; jnt_cnt < m_jointPointers.size(); jnt_cnt++) {
        //TODO: consider multi-dof joint ?
        m_positions[jnt_cnt] = m_jointPointers[jnt_cnt]->GetAngle (0).Degree();
        m_velocities[jnt_cnt] = GazeboYarpPlugins::convertRadiansToDegrees(m_jointPointers[jnt_cnt]->GetVelocity(0));
        m_torques[jnt_cnt] = m_jointPointers[jnt_cnt]->GetForce(0u);
    }

    // Updating timestamp
    //gazebo::common::Time currTime = gazebo::common::Time(_info.simTime.Double());
    /*gazebo::common::Time currTime = m_robot->GetWorld()->GetSimTime();
    gazebo::common::Time stepTime = currTime - previousGazeboSimTime;
    previousGazeboSimTime = currTime;*/
    gazebo::common::Time currTime = gazebo::common::Time(_info.simTime.Double());
    gazebo::common::Time stepTime = currTime - m_lastTimestamp.getTime();
    
    m_lastTimestamp.update(_info.simTime.Double());

    //logger.log(m_velocities[2]);

    for (unsigned int j = 0; j < m_numberOfJoints; ++j) {
        //set pos joint value, set m_referenceVelocities joint value
        if ((m_controlMode[j] == VOCAB_CM_POSITION || m_controlMode[j] == VOCAB_CM_POSITION_DIRECT)
            && (m_interactionMode[j] == VOCAB_IM_STIFF)) {
            if (stepTime  > 0.0) {
                if (m_clock % _T_controller == 0) {
                    if (m_controlMode[j] == VOCAB_CM_POSITION) {
                        computeTrajectory(j);
                        //std::cout<<"\n\nhey\n\n"<<std::endl;
                    }
                    // sendPositionToGazebo(j, m_referencePositions[j]);
                    //if (stepTime > 0.0) {
                        double diff = m_jointPointers[j]->GetAngle(0).Radian() - GazeboYarpPlugins::convertDegreesToRadians(m_referencePositions[j]);
                        //std::cout<<"\n\ndiff: "<< diff <<"\n\n"<<std::endl;
                        //if (j == 5)
                        //std::cout<<"\n\nstep time: "<<stepTime.Double()<<"\n\n"<<std::endl;
                        //if (diff > GazeboYarpPlugins::convertDegreesToRadians(1.0)) {
                            //GazeboYarpControlBoardDriver::PID positionPID = m_positionPIDs[j];
                            //m_gazeboPositionPIDs[j].SetPGain(positionPID.p);
                            //m_gazeboPositionPIDs[j].SetIGain(positionPID.i);
                            //m_gazeboPositionPIDs[j].SetDGain(positionPID.d);
                            double cmd = m_gazeboPositionPIDs[j].Update(diff, stepTime);
                            //std::cout<<"\n\ncmd: "<<cmd<<"\n\n"<<std::endl;
                            m_jointPointers[j]->SetForce(0, cmd);
                        //}
                    //}
                }
            }
        } else if ((m_controlMode[j] == VOCAB_CM_VELOCITY) && (m_interactionMode[j] == VOCAB_IM_STIFF)) {//set vmo joint value
            if (m_clock % _T_controller == 0) {
                sendVelocityToGazebo(j, m_referenceVelocities[j]);
            }
        } else if (m_controlMode[j] == VOCAB_CM_TORQUE) {
            if (m_clock % _T_controller == 0) {
                sendTorqueToGazebo(j, m_referenceTorques[j]);
            }
        } else if (m_controlMode[j] == VOCAB_CM_OPENLOOP) {
            //OpenLoop control sends torques to gazebo at this moment.
            //Check if gazebo implements a "motor" entity and change the code accordingly.
            if (m_clock % _T_controller == 0) {
                sendTorqueToGazebo(j, m_referenceTorques[j]);
            }
        } else if ((m_controlMode[j] == VOCAB_CM_POSITION || m_controlMode[j] == VOCAB_CM_POSITION_DIRECT)
            && (m_interactionMode[j] == VOCAB_IM_COMPLIANT)) {
            if (m_clock % _T_controller == 0) {
                if (m_controlMode[j] == VOCAB_CM_POSITION) {
                    computeTrajectory(j);
                }
                // std::cout<<"\n\n\n\n\n\n\n\nCOMPLIANT\n\n\n\n\n\n\n\n"<<std::endl; // miguel debug
                sendImpPositionToGazebo(j, m_referencePositions[j]);
            }
        }
    }
}

void GazeboYarpControlBoardDriver::setMinMaxPos()
{
    for(unsigned int i = 0; i < m_numberOfJoints; ++i) {
        m_jointLimits[i].max = m_jointPointers[i]->GetUpperLimit(0).Degree();
        m_jointLimits[i].min = m_jointPointers[i]->GetLowerLimit(0).Degree();
    }
}

bool GazeboYarpControlBoardDriver::setJointNames()  //WORKS
{
    yarp::os::Bottle joint_names_bottle = m_pluginParameters.findGroup("jointNames");

    if (joint_names_bottle.isNull()) {
        std::cout << "GazeboYarpControlBoardDriver::setJointNames(): Error cannot find jointNames." << std::endl;
        return false;
    }

    int nr_of_joints = joint_names_bottle.size()-1;

    m_jointNames.resize(nr_of_joints);
    m_jointPointers.resize(nr_of_joints);

    const gazebo::physics::Joint_V & gazebo_models_joints = m_robot->GetJoints();

    for (unsigned int i = 0; i < m_jointNames.size(); i++) {
        bool joint_found = false;
        std::string controlboard_joint_name(joint_names_bottle.get(i+1).asString().c_str());

        for (unsigned int gazebo_joint = 0; gazebo_joint < gazebo_models_joints.size() && !joint_found; gazebo_joint++) {
            std::string gazebo_joint_name = gazebo_models_joints[gazebo_joint]->GetName();
            if (GazeboYarpPlugins::hasEnding(gazebo_joint_name,controlboard_joint_name)) {
                joint_found = true;
                m_jointNames[i] = gazebo_joint_name;
                m_jointPointers[i] = this->m_robot->GetJoint(gazebo_joint_name);
            }
        }

        if (!joint_found) {
            yError() << "GazeboYarpControlBoardDriver::setJointNames(): cannot find joint " << m_jointNames[i]
                     << " ( " << i << " of " << nr_of_joints << " ) " << "\n";
            yError() << "jointNames is " << joint_names_bottle.toString() << "\n";
            m_jointNames.resize(0);
            m_jointPointers.resize(0);
            return false;
        }
    }
    return true;
}

void GazeboYarpControlBoardDriver::setPIDsForGroup(std::string pidGroupName,
                                                   std::vector<GazeboYarpControlBoardDriver::PID>& pids,
                                                   enum PIDFeedbackTerm pidTerms,
                                                   std::vector<gazebo::common::PID>& pidsGazebo)
{
    yarp::os::Property prop;
    if (m_pluginParameters.check(pidGroupName.c_str())) {
        std::cout<<"Found PID information in plugin parameters group " << pidGroupName << std::endl;

        for (unsigned int i = 0; i < m_numberOfJoints; ++i) {
            std::stringstream property_name;
            property_name<<"Pid";
            property_name<<i;

            yarp::os::Bottle& pid = m_pluginParameters.findGroup(pidGroupName.c_str()).findGroup(property_name.str().c_str());

            GazeboYarpControlBoardDriver::PID pidValue = {0, 0, 0, -1, -1};
            gazebo::common::PID* pidController = new gazebo::common::PID(1, 0.1, 0.01, 1, -1, 1000, -1000);
            if (pidTerms & PIDFeedbackTermProportionalTerm)
                pidValue.p = pid.get(1).asDouble();
                pidController->SetPGain(pid.get(1).asDouble());
            if (pidTerms & PIDFeedbackTermDerivativeTerm)
                pidValue.d = pid.get(2).asDouble();
                pidController->SetDGain(pid.get(2).asDouble());
            if (pidTerms & PIDFeedbackTermIntegrativeTerm)
                pidValue.i = pid.get(3).asDouble();
                pidController->SetIGain(pid.get(3).asDouble());

            pidValue.maxInt = pid.get(4).asDouble();
            pidValue.maxOut = pid.get(5).asDouble();

            pidController->SetIMin(-1);
            pidController->SetIMax(1);
            pidController->SetCmdMin(-1000);
            pidController->SetCmdMax(1000);

            pidsGazebo.push_back(*pidController);
            pids.push_back(pidValue);
            std::cout<<"  P: "<<pidValue.p<<" I: "<<pidValue.i<<" D: "<<pidValue.d<<" maxInt: "<<pidValue.maxInt<<" maxOut: "<<pidValue.maxOut<<std::endl;
        }
        std::cout<<"OK!"<<std::endl;
    } else {
        double default_p = pidTerms & PIDFeedbackTermProportionalTerm ? 500.0 : 0;
        double default_i = pidTerms & PIDFeedbackTermIntegrativeTerm ? 0.1 : 0;
        double default_d = pidTerms & PIDFeedbackTermDerivativeTerm ? 1.0 : 0;
        std::cout<<"PID gain information not found in plugin parameters, using default gains ( "
        <<"P " << default_p << " I " << default_i << " D " << default_d << " )" <<std::endl;
        for (unsigned int i = 0; i < m_numberOfJoints; ++i) {
            GazeboYarpControlBoardDriver::PID pid = {500, 0.1, 1.0, -1, -1};
            pids.push_back(pid);
        }
    }
}

void GazeboYarpControlBoardDriver::setMinMaxImpedance()
{

    yarp::os::Bottle& name_bot = m_pluginParameters.findGroup("WRAPPER").findGroup("networks");
    std::string name = name_bot.get(1).toString();

    yarp::os::Bottle& kin_chain_bot = m_pluginParameters.findGroup(name);
    if (kin_chain_bot.check("min_stiffness")) {
        std::cout<<"min_stiffness param found!"<<std::endl;
        yarp::os::Bottle& min_stiff_bot = kin_chain_bot.findGroup("min_stiffness");
        if(min_stiff_bot.size()-1 == m_numberOfJoints) {
            for(unsigned int i = 0; i < m_numberOfJoints; ++i)
                m_minStiffness[i] = min_stiff_bot.get(i+1).asDouble();
        } else
            std::cout<<"Invalid number of params"<<std::endl;
    } else
        std::cout<<"No minimum stiffness value found in ini file, default one will be used!"<<std::endl;

    if (kin_chain_bot.check("max_stiffness")) {
        std::cout<<"max_stiffness param found!"<<std::endl;
        yarp::os::Bottle& max_stiff_bot = kin_chain_bot.findGroup("max_stiffness");
        if (max_stiff_bot.size()-1 == m_numberOfJoints) {
            for (unsigned int i = 0; i < m_numberOfJoints; ++i)
                m_maxStiffness[i] = max_stiff_bot.get(i+1).asDouble();
        } else
            std::cout<<"Invalid number of params"<<std::endl;
    }
    else
        std::cout<<"No maximum stiffness value found in ini file, default one will be used!"<<std::endl;

    if (kin_chain_bot.check("min_damping")) {
        std::cout<<"min_damping param found!"<<std::endl;
        yarp::os::Bottle& min_damping_bot = kin_chain_bot.findGroup("min_damping");
        if(min_damping_bot.size()-1 == m_numberOfJoints) {
            for(unsigned int i = 0; i < m_numberOfJoints; ++i)
                m_minDamping[i] = min_damping_bot.get(i+1).asDouble();
        } else
            std::cout<<"Invalid number of params"<<std::endl;
    } else
        std::cout<<"No minimum dampings value found in ini file, default one will be used!"<<std::endl;

    if(kin_chain_bot.check("max_damping")) {
        std::cout<<"max_damping param found!"<<std::endl;
        yarp::os::Bottle& max_damping_bot = kin_chain_bot.findGroup("max_damping");
        if (max_damping_bot.size() - 1 == m_numberOfJoints) {
            for(unsigned int i = 0; i < m_numberOfJoints; ++i)
                m_maxDamping[i] = max_damping_bot.get(i+1).asDouble();
        } else
            std::cout<<"Invalid number of params"<<std::endl;
    } else
        std::cout<<"No maximum damping value found in ini file, default one will be used!"<<std::endl;

    std::cout<<"min_stiffness: [ "<<m_minStiffness.toString()<<" ]"<<std::endl;
    std::cout<<"max_stiffness: [ "<<m_maxStiffness.toString()<<" ]"<<std::endl;
    std::cout<<"min_damping: [ "<<m_minDamping.toString()<<" ]"<<std::endl;
    std::cout<<"max_damping: [ "<<m_maxDamping.toString()<<" ]"<<std::endl;
}

void GazeboYarpControlBoardDriver::setPIDs()
{
    setPIDsForGroup("GAZEBO_PIDS", m_positionPIDs, PIDFeedbackTermAllTerms, m_gazeboPositionPIDs);
    setPIDsForGroup("GAZEBO_VELOCITY_PIDS", m_velocityPIDs, PIDFeedbackTerm(PIDFeedbackTermProportionalTerm | PIDFeedbackTermIntegrativeTerm), m_gazeboVelocityPIDs);
    setPIDsForGroup("GAZEBO_IMPEDANCE_POSITION_PIDS", m_impedancePosPDs, PIDFeedbackTerm(PIDFeedbackTermProportionalTerm | PIDFeedbackTermDerivativeTerm), m_gazeboImpedancePosPIDs);
}

bool GazeboYarpControlBoardDriver::sendPositionsToGazebo(Vector &refs)
{
    for (unsigned int j=0; j<m_numberOfJoints; j++) {
        sendPositionToGazebo(j,refs[j]);
    }
    return true;
}

bool GazeboYarpControlBoardDriver::sendPositionToGazebo(int j,double ref)
{
    gazebo::msgs::JointCmd j_cmd;
    prepareJointMsg(j_cmd,j,ref);
    m_jointCommandPublisher->WaitForConnection();
    m_jointCommandPublisher->Publish(j_cmd);
    return true;
}

void GazeboYarpControlBoardDriver::prepareJointMsg(gazebo::msgs::JointCmd& j_cmd, const int joint_index, const double ref)  //WORKS
{
    GazeboYarpControlBoardDriver::PID positionPID = m_positionPIDs[joint_index];

    j_cmd.set_name(m_jointPointers[joint_index]->GetScopedName());
    j_cmd.mutable_position()->set_target(GazeboYarpPlugins::convertDegreesToRadians(ref));
    j_cmd.mutable_position()->set_p_gain(positionPID.p);
    j_cmd.mutable_position()->set_i_gain(positionPID.i);
    j_cmd.mutable_position()->set_d_gain(positionPID.d);
}

bool GazeboYarpControlBoardDriver::sendVelocitiesToGazebo(yarp::sig::Vector& refs) //NOT TESTED
{
    for (unsigned int j = 0; j < m_numberOfJoints; j++) {
        sendVelocityToGazebo(j,refs[j]);
    }
    return true;
}

bool GazeboYarpControlBoardDriver::sendVelocityToGazebo(int j,double ref) //NOT TESTED
{
    /* SetVelocity method */
    /*gazebo::physics::JointPtr joint =  this->m_robot->GetJoint(m_jointNames[j]);
     joint->SetMaxForce(0, joint->GetEffortLimit(0)*1.1); //<-- MAGIC NUMBER!!!!
     //      std::cout<<"MaxForce:" <<joint->GetMaxForce(0)<<std::endl;
     joint->SetVelocity(0,toRad(ref));
     */
    /* JointController method. If you pick this control method for control
     *      of joint velocities, you should also take care of the switching logic
     *      in setVelocityMode, setTorqueMode and setPositionMode:
     *      that is, the SetMarxForce(0,0) and SetVelocity(0,0) are no longer
     *      needed, but the JointController::AddJoint() method needs to be called
     *      when you switch to velocity mode, to make sure the PIDs get reset */
    gazebo::msgs::JointCmd j_cmd;
    prepareJointVelocityMsg(j_cmd,j,ref);
    m_jointCommandPublisher->WaitForConnection();
    m_jointCommandPublisher->Publish(j_cmd);
    /**/
    return true;
}

void GazeboYarpControlBoardDriver::prepareJointVelocityMsg(gazebo::msgs::JointCmd& j_cmd, const int j, const double ref) //NOT TESTED
{
    GazeboYarpControlBoardDriver::PID velocityPID = m_velocityPIDs[j];

    j_cmd.set_name(m_jointPointers[j]->GetScopedName());
//    j_cmd.mutable_position()->set_p_gain(0.0);
//    j_cmd.mutable_position()->set_i_gain(0.0);
//    j_cmd.mutable_position()->set_d_gain(0.0);
    j_cmd.mutable_velocity()->set_p_gain(velocityPID.p);
    j_cmd.mutable_velocity()->set_i_gain(velocityPID.i);
    j_cmd.mutable_velocity()->set_d_gain(velocityPID.d);
    //     if (velocityPID.maxInt > 0) {
    //         j_cmd.mutable_velocity()->set_i_max(velocityPID.maxInt);
    //         j_cmd.mutable_velocity()->set_i_min(-velocityPID.maxInt);
    //     }
    //     if (velocityPID.maxOut > 0) {
    //         j_cmd.mutable_velocity()->set_limit(velocityPID.maxOut);
    //     }

    j_cmd.mutable_velocity()->set_target(GazeboYarpPlugins::convertDegreesToRadians(ref));
}

bool GazeboYarpControlBoardDriver::sendTorquesToGazebo(yarp::sig::Vector& refs) //NOT TESTED
{
    for (unsigned int j = 0; j < m_numberOfJoints; j++) {
        sendTorqueToGazebo(j,refs[j]);
    }
    return true;
}

bool GazeboYarpControlBoardDriver::sendTorqueToGazebo(const int j,const double ref) //NOT TESTED
{
    gazebo::msgs::JointCmd j_cmd;
    prepareJointTorqueMsg(j_cmd,j,ref);
    m_jointCommandPublisher->WaitForConnection();
    m_jointCommandPublisher->Publish(j_cmd);
    return true;
}

void GazeboYarpControlBoardDriver::prepareJointTorqueMsg(gazebo::msgs::JointCmd& j_cmd, const int j, const double ref) //NOT TESTED
{
    j_cmd.set_name(m_jointPointers[j]->GetScopedName());
//    j_cmd.mutable_position()->set_p_gain(0.0);
//    j_cmd.mutable_position()->set_i_gain(0.0);
//    j_cmd.mutable_position()->set_d_gain(0.0);
//    j_cmd.mutable_velocity()->set_p_gain(0.0);
//    j_cmd.mutable_velocity()->set_i_gain(0.0);
//    j_cmd.mutable_velocity()->set_d_gain(0.0);
    j_cmd.set_force(ref);
}

void GazeboYarpControlBoardDriver::sendImpPositionToGazebo ( const int j, const double des )
{
    if(j >= 0 && j < m_numberOfJoints) {
        /*
         Here joint positions and speeds are in [deg] and [deg/sec].
         Therefore also stiffness and damping has to be [Nm/deg] and [Nm*sec/deg].
         */
        //std::cout<<"m_velocities"<<j<<" : "<<m_velocities[j]<<std::endl;
        double q = m_positions[j] - m_zeroPosition[j];
        double t_ref = -m_impedancePosPDs[j].p * (q - des) - m_impedancePosPDs[j].d * m_velocities[j] + m_torqueOffsett[j];
        sendTorqueToGazebo(j, t_ref);
    }
}

void GazeboYarpControlBoardDriver::sendImpPositionsToGazebo ( Vector &dess )
{
    for(unsigned int i = 0; i < m_numberOfJoints; ++i)
        sendImpPositionToGazebo(i, dess[i]);
}
