#include <assert.h>

#include "xmlConfig.h"
#include "debug.h"

using namespace PowerAPI;

XmlConfig::XmlConfig( std::string file ) 
{
	DBGX("config file `%s`\n",file.c_str());
    m_xml = new tinyxml2::XMLDocument();
    m_xml->LoadFile( file.c_str() );
	m_systemNode = findNode( m_xml->RootElement(), "System" ); 
	assert( m_systemNode );
}

void XmlConfig::print( std::ostream& out  )
{
	printTree( out, m_xml->RootElement() );
}

void XmlConfig::printTree( std::ostream& out, XMLNode* node )
{
    XMLElement* el = static_cast<XMLElement*>(node);

    if ( NULL == el ) return;

    if ( ! strcmp(el->Name(),"obj") ) {
		out << el->Name() << " type="<< el->Attribute("type") <<
							" name "<<  el->Attribute("name") << std::endl;   
    } else if ( ! strcmp(el->Name(),"child") ) {
		out << el->Name() << " name=" << el->Attribute("name") << std::endl;
    } else if ( ! strcmp(el->Name(),"attr") ) {
		out << el->Name() << " name=" << el->Attribute("name") <<
							" op=" << el->Attribute("op") << std::endl;
    } else {
#if 0
		out << el->Name() << std::endl;
#endif
    }

    if ( ! node->NoChildren() ) {
        XMLNode* tmp = node->FirstChild();
        do {
            printTree( out, tmp );
        } while ( ( tmp = tmp->NextSibling() ) );
    }

}

bool XmlConfig::hasObject( const std::string name ) 
{
	DBGX("find %s\n",name.c_str());
	return ( NULL != findObject( name ) );
}

PWR_ObjType XmlConfig::objType( const std::string name )
{
	return getType( findObject( name ) );
}

PWR_ObjType XmlConfig::getType( XMLElement* elm )
{
	return objTypeStrToInt( elm->Attribute("type") );
}

std::deque< Config::Plugin > XmlConfig::findPlugins( )
{
	std::deque< Config::Plugin > ret;
	
	XMLNode* node = findNodes1stChild( m_systemNode->FirstChild(), "Plugins" );

	while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		assert( ! strcmp( elm->Name(), "plugin" ) );

		DBGX("name=`%s` lib=`%s`\n",elm->Attribute("name"),
											elm->Attribute("lib"));

		Config::Plugin plugin;
		plugin.name = elm->Attribute("name"); 
		plugin.lib = elm->Attribute("lib");
		ret.push_back( plugin );

        node = node->NextSibling();
	}
	return ret;
}

std::deque< Config::SysDev > XmlConfig::findSysDevs()
{
	std::deque< Config::SysDev > ret;

	XMLNode* node = findNodes1stChild( m_systemNode->FirstChild(), "Devices" );

	while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		assert( ! strcmp( elm->Name(), "device" ) );

		Config::SysDev dev;
		dev.name = elm->Attribute("name");
		dev.plugin = elm->Attribute("plugin");
		dev.initString = elm->Attribute("initString");
		ret.push_back( dev );

        node = node->NextSibling();
	}

	return ret;
}

std::deque< Config::ObjDev > 
			XmlConfig::findObjDevs( std::string name, PWR_AttrName attr )
{
	DBGX("%s %s\n",name.c_str(), attrNameToString(attr).c_str());

	std::deque< Config::ObjDev > ret;

	XMLElement* obj = findObject( name );
	assert( obj );
	XMLNode* node = findAttr( obj, attrNameToString(attr) );		
	if ( ! node ) return ret;

	node = node->FirstChild();
    while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		assert( ! strcmp( elm->Name(), "src" ) );

        if ( 0 == strcmp( elm->Attribute("type"), "device") ) {
			DBGX("%s\n",elm->Attribute("name") );
			std::string tmp = elm->Attribute("name");
			XMLElement* devElm = static_cast<XMLElement*>(findDev( obj, tmp ));
			assert( devElm );

			Config::ObjDev dev;

			dev.device = devElm->Attribute("device");
			dev.openString = devElm->Attribute("openString");
			ret.push_back( dev );

			DBGX("%s %s\n", dev.device.c_str(), dev.openString.c_str() );
        }

        node = node->NextSibling();
    }

	return ret;
}

std::deque< std::string >
        XmlConfig::findAttrChildren( std::string name, PWR_AttrName attr )
{
	DBGX("%s %s\n",name.c_str(), attrNameToString(attr).c_str());

	std::deque< std::string > ret;

	XMLElement* obj = findObject( name );
	assert( obj );
	XMLNode* node = findAttr( obj, attrNameToString(attr) );		
	if ( ! node ) return ret;

	node = node->FirstChild();
    while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		assert( ! strcmp( elm->Name(), "src" ) );

        if ( 0 == strcmp( elm->Attribute("type"), "child") ) {
			DBGX("%s\n",elm->Attribute("name") );
			std::string name = obj->Attribute("name");
			name += ".";
			name += elm->Attribute("name");			
			ret.push_back( name ); 
			DBGX("%s\n",ret.back().c_str());
        }

        node = node->NextSibling();
    }

	return ret;
}

std::string XmlConfig::findAttrOp( std::string name, PWR_AttrName attr )
{
	DBGX("%s %s\n",name.c_str(), attrNameToString(attr).c_str());

	XMLElement* obj = findObject( name );
	assert( obj );

	XMLNode* node = findAttr( obj, attrNameToString(attr) );		
	if ( NULL == node ) {
		return "";
	}else {
		return static_cast<XMLElement*>(node)->Attribute("op");
	}
}

std::deque< std::string > XmlConfig::findChildren( std::string name )
{
	DBGX("%s\n",name.c_str() );

	std::deque< std::string > ret;

	XMLElement* obj = findObject( name );
	assert( obj );

	XMLNode* node = findNodes1stChild( obj->FirstChild(), "children" );

    while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		assert( ! strcmp( elm->Name(), "child" ) );

		std::string tmp = obj->Attribute("name");
		tmp += ".";
		tmp += elm->Attribute("name");			
		ret.push_back( tmp ); 
		DBGX("found child %s\n",ret.back().c_str());

        node = node->NextSibling();
    }

	return ret;
}

std::deque< std::string > XmlConfig::findObjType( PWR_ObjType type )
{
	std::deque< std::string > ret;

	std::string typeStr = objTypeToString( type ); 
	XMLNode* node = findNodes1stChild( m_systemNode->FirstChild(), "Objects" );

	while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		assert( ! strcmp( elm->Name(), "obj" ) );

		DBGX("name=`%s` type=`%s`\n",elm->Attribute("name"),
											elm->Attribute("type"));

		if ( ! typeStr.compare( elm->Attribute("type") ) ) {
			ret.push_back( elm->Attribute("name") );
		}

        node = node->NextSibling();
	}

	return ret;
}

XMLNode* XmlConfig::findDev( XMLElement* elm, std::string name )
{
	return findNodeWithAttr( elm, "devices", "name", name );
} 

XMLNode* XmlConfig::findAttr( XMLElement* elm, std::string attr )
{
	return findNodeWithAttr( elm, "attributes", "name", attr );
} 

XMLNode* XmlConfig::findNodeWithAttr( XMLElement* elm, 
	std::string nodeName, std::string attrName, std::string attrValue )
{
    XMLNode* node = elm->FirstChild();

	DBGX("nodeName=%s attrName=%s attrValue=%s\n",nodeName.c_str(), 
			attrName.c_str(), attrValue.c_str());

    // find the attributes element
    while ( node ) {
        elm = static_cast<XMLElement*>(node);

        if ( 0 == nodeName.compare( elm->Name() ) ) {
            node = elm->FirstChild();
            break;
        }
        node = node->NextSibling();
    }

	while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		if ( 0 == attrValue.compare( elm->Attribute(attrName.c_str())) ) {
			DBGX("found %s\n",elm->Attribute(attrName.c_str()));
			break;
		}

        node = node->NextSibling();
	} 
			
	return node;
} 

XMLNode* XmlConfig::findNodes1stChild( XMLNode* node, const std::string name )
{
	node = findNode( node, name );
	assert( node );
	return node->FirstChild();
}

XMLElement* XmlConfig::findObject( const std::string name )
{
    DBGX("`%s`\n",name.c_str());

	XMLNode* node = findNodes1stChild( m_systemNode->FirstChild(), "Objects" );

    while ( node ) {
        XMLElement* el = static_cast<XMLElement*>(node);

#if 0 
        DBGX("%s %s name=`%s`\n",el->Name(),
                el->Attribute("type"), el->Attribute("name"));
#endif

        if ( 0 == name.compare( el->Attribute("name") ) ) {
            return el;
        }
        node = node->NextSibling();
    }

    return NULL;
}

XMLNode* XmlConfig::findNode( XMLNode* node, const std::string name )
{
	DBGX("root=%s find node named `%s`\n", 
			static_cast<XMLElement*>(node)->Name(), name.c_str());

    // find the plugins element
    while ( node ) {
        XMLElement* elm = static_cast<XMLElement*>(node);

		DBGX("current=`%s`\n", elm->Name() );
        if ( 0 == name.compare( elm->Name() ) ) {
			DBGX("found node named `%s`\n", name.c_str());
            break;
        }
        node = node->NextSibling();
    }
	return node; 
}

std::string XmlConfig::objTypeToString( PWR_ObjType type )
{
    switch( type ) {
    case PWR_OBJ_PLATFORM: return "Platform";
    case PWR_OBJ_CABINET:  return "Cabinet";
    case PWR_OBJ_BOARD:    return "Board";
    case PWR_OBJ_NODE:     return "Node";
    case PWR_OBJ_SOCKET:   return "Socket";
    case PWR_OBJ_CORE:     return "Core";
    case PWR_OBJ_NIC:      return "Nic";
    case PWR_OBJ_MEM:      return "Memory";
    case PWR_OBJ_INVALID:  return "Invalid";
    }
    return NULL;
}

PWR_ObjType XmlConfig::objTypeStrToInt( const std::string name )
{
    if ( 0 == name.compare( "Platform" ) ) {
        return  PWR_OBJ_PLATFORM;
    } else if ( 0 == name.compare( "Cabinet" ) ) {
        return PWR_OBJ_CABINET;
    } else if ( 0 == name.compare( "Board" ) ) {
        return PWR_OBJ_BOARD;
    } else if ( 0 == name.compare( "Node" ) ) {
        return PWR_OBJ_NODE;
    } else if ( 0 == name.compare( "Socket" ) ) {
        return PWR_OBJ_SOCKET;
    } else if ( 0 == name.compare( "Core" ) ) {
        return PWR_OBJ_CORE;
    }
    return PWR_OBJ_INVALID;
}

std::string XmlConfig::attrNameToString( PWR_AttrName name )
{
    switch( name ){
    case PWR_ATTR_NAME: return "Name";
    case PWR_ATTR_FREQ: return "Freq";
    case PWR_ATTR_PSTATE: return "Pstate";
    case PWR_ATTR_MAX_POWER: return "Max Power";
    case PWR_ATTR_MIN_POWER: return "Min Power";
    case PWR_ATTR_AVG_POWER: return "Avg Power";
    case PWR_ATTR_POWER: return "POWER";
    case PWR_ATTR_VOLTAGE: return "VOLTAGE";
    case PWR_ATTR_CURRENT: return "CURRENT";
    case PWR_ATTR_ENERGY: return "Energy";
    case PWR_ATTR_INVALID: return "Invalid";
    default: return "????";
    }
    return NULL;
}