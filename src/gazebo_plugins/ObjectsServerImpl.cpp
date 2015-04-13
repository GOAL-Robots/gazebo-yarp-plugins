/*
 * Copyright (C) 2007-2015 Istituto Italiano di Tecnologia. RBCS Department.
 * Author: Silvio Traversaro
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 */

#include "ObjectsServerImpl.h"
#include "Objects.hh"
#include <iostream>

namespace gazebo {

    ObjectsServerImpl::ObjectsServerImpl(GazeboYarpObjects& objectsPlugin)
    : m_objects(objectsPlugin) {}

bool ObjectsServerImpl::attach(const std::string& link_name, const std::string& object_name)
{
    return m_objects.attach(link_name, object_name);
}

bool ObjectsServerImpl::detach(const std::string& link_name, const std::string& object_name)
{
    return m_objects.detach(link_name, object_name);
}

}
