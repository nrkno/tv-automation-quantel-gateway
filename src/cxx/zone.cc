#include "zone.h"

void testConnectionExecute(napi_env env, void* data) {
	testConnectionCarrier* c = (testConnectionCarrier*) data;
	CORBA::ORB_var orb;
	Quentin::ZonePortal::_ptr_type zp;

	try {
		resolveZonePortal(c->isaIOR, &orb, &zp);

		c->zoneNumber = zp->getZoneNumber();
	}
	catch(CORBA::SystemException& ex) {
		NAPI_REJECT_SYSTEM_EXCEPTION(ex);
	}
	catch(CORBA::Exception& ex) {
		NAPI_REJECT_CORBA_EXCEPTION(ex);
	}
	catch(omniORB::fatalException& fe) {
		NAPI_REJECT_FATAL_EXCEPTION(fe);
	}

	orb->destroy();
}

void testConnectionComplete(napi_env env, napi_status asyncStatus, void* data) {
  testConnectionCarrier* c = (testConnectionCarrier*) data;
	napi_value result;

	if (asyncStatus != napi_ok) {
		c->status = asyncStatus;
		c->errorMsg = "Test connection failed to complete.";
	}
	REJECT_STATUS;

	c->status = napi_create_string_utf8(env, "PONG!", NAPI_AUTO_LENGTH, &result);
	REJECT_STATUS;

	napi_status status;
	status = napi_resolve_deferred(env, c->_deferred, result);
	FLOATING_STATUS;

	tidyCarrier(env, c);
}

napi_value testConnection(napi_env env, napi_callback_info info) {
  testConnectionCarrier* c = new testConnectionCarrier;
  napi_value promise, resourceName;
	char* isaIOR = nullptr;
	size_t iorLen = 0;

  c->status = napi_create_promise(env, &c->_deferred, &promise);
  REJECT_RETURN;

	size_t argc = 1;
	napi_value argv[1];
	c->status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
	REJECT_RETURN;

	if (argc < 1) {
		REJECT_ERROR_RETURN("Connection test must be provided with a IOR reference to an ISA server.",
	    QGW_INVALID_ARGS);
	}
	c->status = napi_get_value_string_utf8(env, argv[0], nullptr, 0, &iorLen);
	REJECT_RETURN;
	isaIOR = (char*) malloc((iorLen + 1) * sizeof(char));
	c->status = napi_get_value_string_utf8(env, argv[0], isaIOR, iorLen + 1, &iorLen);
	REJECT_RETURN;

	c->isaIOR = isaIOR;

	c->status = napi_create_string_utf8(env, "TestConnection", NAPI_AUTO_LENGTH, &resourceName);
  REJECT_RETURN;
  c->status = napi_create_async_work(env, nullptr, resourceName, testConnectionExecute,
    testConnectionComplete, c, &c->_request);
  REJECT_RETURN;
  c->status = napi_queue_async_work(env, c->_request);
  REJECT_RETURN;

  return promise;
}

void listZonesExecute(napi_env env, void* data) {
	listZonesCarrier* c = (listZonesCarrier*) data;
	CORBA::ORB_var orb;
	Quentin::ZonePortal::_ptr_type zp;

	try {
		resolveZonePortal(c->isaIOR, &orb, &zp);

		c->zoneIDs = zp->getZones(false);
		c->zoneIDs->length(c->zoneIDs->length() + 1);
		for ( int x = c->zoneIDs->length() - 1 ; x > 0 ; x-- ) {
			c->zoneIDs[x] = c->zoneIDs[x - 1];
		}
		c->zoneIDs[0] = zp->getZoneNumber(); // Always start with the local/default zone

		c->zoneNames = (CORBA::WChar**) malloc(c->zoneIDs->length() * sizeof(CORBA::WChar*));
		c->remotes = (CORBA::Boolean*) malloc(c->zoneIDs->length() * sizeof(CORBA::Boolean));

		for ( uint32_t x = 0 ; x < c->zoneIDs->length() ; x++ ) {
			c->zoneNames[x] = zp->getZoneName(c->zoneIDs[x]);
			c->remotes[x] = zp->zoneIsRemote(c->zoneIDs[x]);
		}
	}
	catch(CORBA::SystemException& ex) {
		NAPI_REJECT_SYSTEM_EXCEPTION(ex);
	}
	catch(CORBA::Exception& ex) {
		NAPI_REJECT_CORBA_EXCEPTION(ex);
	}
	catch(omniORB::fatalException& fe) {
		NAPI_REJECT_FATAL_EXCEPTION(fe);
	}

	orb->destroy();
}

void listZonesComplete(napi_env env, napi_status asyncStatus, void* data) {
	listZonesCarrier* c = (listZonesCarrier*) data;
	napi_value result, item, prop;
	CORBA::WChar* zoneName;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;

	if (asyncStatus != napi_ok) {
		c->status = asyncStatus;
		c->errorMsg = "List zones failed to complete.";
	}
	REJECT_STATUS;

	c->status = napi_create_array(env, &result);
	REJECT_STATUS;

	for ( uint32_t x = 0 ; x < c->zoneIDs->length() ; x++ ) {
		c->status = napi_create_object(env, &item);
		REJECT_STATUS;

		zoneName = c->zoneNames[x];

		c->status = napi_create_string_utf8(env, "ZonePortal", NAPI_AUTO_LENGTH, &prop);
		REJECT_STATUS;
		c->status = napi_set_named_property(env, item, "type", prop);
		REJECT_STATUS;

		c->status = napi_create_int32(env, c->zoneIDs[x], &prop);
		REJECT_STATUS;
		c->status = napi_set_named_property(env, item, "zoneNumber", prop);
		REJECT_STATUS;

		std::string zoneNameStr = utf8_conv.to_bytes(zoneName);

		c->status = napi_create_string_utf8(env, zoneNameStr.c_str(), NAPI_AUTO_LENGTH, &prop);
		REJECT_STATUS;
		c->status = napi_set_named_property(env, item, "zoneName", prop);
		REJECT_STATUS;

		c->status = napi_get_boolean(env, c->remotes[x], &prop);
		REJECT_STATUS;
		c->status = napi_set_named_property(env, item, "isRemote", prop);
		REJECT_STATUS;

		c->status = napi_set_element(env, result, x, item);
		REJECT_STATUS;
	}

	napi_status status;
	status = napi_resolve_deferred(env, c->_deferred, result);
	FLOATING_STATUS;

	tidyCarrier(env, c);
}

napi_value listZones(napi_env env, napi_callback_info info) {
	listZonesCarrier* c = new listZonesCarrier;
	napi_value promise, resourceName;
	char* isaIOR = nullptr;
	size_t iorLen = 0;

	c->status = napi_create_promise(env, &c->_deferred, &promise);
  REJECT_RETURN;

	size_t argc = 1;
	napi_value argv[1];
	c->status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
	REJECT_RETURN;

	if (argc < 1) {
		REJECT_ERROR_RETURN("Connection test must be provided with a IOR reference to an ISA server.",
	    QGW_INVALID_ARGS);
	}
	c->status = napi_get_value_string_utf8(env, argv[0], nullptr, 0, &iorLen);
	REJECT_RETURN;
	isaIOR = (char*) malloc((iorLen + 1) * sizeof(char));
	c->status = napi_get_value_string_utf8(env, argv[0], isaIOR, iorLen + 1, &iorLen);
	REJECT_RETURN;

	c->isaIOR = isaIOR;

	c->status = napi_create_string_utf8(env, "ListZones", NAPI_AUTO_LENGTH, &resourceName);
	REJECT_RETURN;
	c->status = napi_create_async_work(env, nullptr, resourceName, listZonesExecute,
		listZonesComplete, c, &c->_request);
	REJECT_RETURN;
	c->status = napi_queue_async_work(env, c->_request);
	REJECT_RETURN;

	return promise;
}

void getServersExecute(napi_env env, void* data) {
	getServersCarrier* c = (getServersCarrier*) data;
	CORBA::ORB_var orb;
	Quentin::ZonePortal::_ptr_type zp;
	serverDetails* server;
	Quentin::Server_var qserver;
	Quentin::ServerInfo_var serverInfo;
	Quentin::WStrings_var portNames, chanPorts;
	std::string portName;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
	Quentin::Longs_var serverIDs;

	try {
		resolveZonePortal(c->isaIOR, &orb, &zp);

		serverIDs = zp->getServers(true);
		for ( uint32_t x = 0 ; x < serverIDs->length() ; x++ ) {
			server = new serverDetails;
			server->ident = serverIDs[x];
			if (serverIDs[x] > 0) { // server is online
				qserver = zp->getServer(serverIDs[x]);
				serverInfo = qserver->getServerInfo();
				server->down = serverInfo->down;
				server->name = utf8_conv.to_bytes(serverInfo->name);
				server->numChannels = serverInfo->numChannels;
				for ( uint32_t y = 0 ; y < serverInfo->pools.length() ; y++ ) {
					server->pools.push_back(serverInfo->pools[y]);
				}
				portNames = qserver->getPortNames();
				for ( uint32_t y = 0 ; y < portNames->length() ; y++ ) {
					server->portNames.push_back(utf8_conv.to_bytes(portNames[y]));
				}
				chanPorts = qserver->getChanPorts();
				for ( uint32_t y = 0 ; y < chanPorts->length() ; y++ ) {
					server->chanPorts.push_back(utf8_conv.to_bytes(chanPorts[y]));
				}
			}
			c->servers.push_back(server);
		}
	}
	catch(CORBA::SystemException& ex) {
		NAPI_REJECT_SYSTEM_EXCEPTION(ex);
	}
	catch(CORBA::Exception& ex) {
		NAPI_REJECT_CORBA_EXCEPTION(ex);
	}
	catch(omniORB::fatalException& fe) {
		NAPI_REJECT_FATAL_EXCEPTION(fe);
	}

	orb->destroy();
}

void getServersComplete(napi_env env, napi_status asyncStatus, void* data) {
	getServersCarrier* c = (getServersCarrier*) data;
	napi_value result, item, prop, subprop;
	serverDetails* server;

	if (asyncStatus != napi_ok) {
		c->status = asyncStatus;
		c->errorMsg = "Test connection failed to complete.";
	}
	REJECT_STATUS;

  c->status = napi_create_array(env, &result);
  REJECT_STATUS;

  for ( uint32_t x = 0 ; x < c->servers.size() ; x++ ) {
    c->status = napi_create_object(env, &item);
    REJECT_STATUS;

    c->status = napi_create_string_utf8(env, "Server", NAPI_AUTO_LENGTH, &prop);
    REJECT_STATUS;
    c->status = napi_set_named_property(env, item, "type", prop);
    REJECT_STATUS;

		int64_t ident = c->servers.at(x)->ident >= 0 ? c->servers.at(x)->ident : -c->servers.at(x)->ident;
    c->status = napi_create_int64(env, ident, &prop);
    REJECT_STATUS;
    c->status = napi_set_named_property(env, item, "ident", prop);
    REJECT_STATUS;

    if (c->servers.at(x)->ident > 0) { // server is online
      server = c->servers.at(x);

      c->status = napi_get_boolean(env, server->down, &prop);
      REJECT_STATUS;
      c->status = napi_set_named_property(env, item, "down", prop);
      REJECT_STATUS;

      c->status = napi_create_string_utf8(env, server->name.c_str(), NAPI_AUTO_LENGTH, &prop);
      REJECT_STATUS;
      c->status = napi_set_named_property(env, item, "name", prop);
      REJECT_STATUS;

      c->status = napi_create_int32(env, server->numChannels, &prop);
      REJECT_STATUS;
      c->status = napi_set_named_property(env, item, "numChannels", prop);
      REJECT_STATUS;

      c->status = napi_create_array(env, &prop);
      REJECT_STATUS;
			uint32_t elementCount = 0;
      for ( auto it = server->pools.cbegin() ; it != server->pools.cend() ; it++ ) {
        c->status = napi_create_int32(env, *it, &subprop);
        REJECT_STATUS;
        c->status = napi_set_element(env, prop, elementCount++, subprop);
        REJECT_STATUS;
      }
      c->status = napi_set_named_property(env, item, "pools", prop);
      REJECT_STATUS;

			c->status = napi_create_array(env, &prop);
      REJECT_STATUS;
			elementCount = 0;
      for ( auto it = server->portNames.cbegin() ; it != server->portNames.cend() ; it++ ) {
        c->status = napi_create_string_utf8(env, (*it).c_str(), NAPI_AUTO_LENGTH, &subprop);
        REJECT_STATUS;
        c->status = napi_set_element(env, prop, elementCount++, subprop);
        REJECT_STATUS;
      }
      c->status = napi_set_named_property(env, item, "portNames", prop);
      REJECT_STATUS;

			c->status = napi_create_array(env, &prop);
			REJECT_STATUS;
			elementCount = 0;
			for ( auto it = server->chanPorts.cbegin() ; it != server->chanPorts.cend() ; it++ ) {
				c->status = napi_create_string_utf8(env, (*it).c_str(), NAPI_AUTO_LENGTH, &subprop);
				REJECT_STATUS;
				c->status = napi_set_element(env, prop, elementCount++, subprop);
				REJECT_STATUS;
			}
			c->status = napi_set_named_property(env, item, "chanPorts", prop);
			REJECT_STATUS;
    } else {
      c->status = napi_get_boolean(env, true, &prop);
      REJECT_STATUS;
      c->status = napi_set_named_property(env, item, "down", prop);
      REJECT_STATUS;
    }

    c->status = napi_set_element(env, result, x, item);
    REJECT_STATUS;
  }

	napi_status status;
	status = napi_resolve_deferred(env, c->_deferred, result);
	FLOATING_STATUS;

	tidyCarrier(env, c);
}

napi_value getServers(napi_env env, napi_callback_info info) {
	getServersCarrier* c =  new getServersCarrier;
	napi_value promise, resourceName;
	char* isaIOR = nullptr;
	size_t iorLen = 0;

  c->status = napi_create_promise(env, &c->_deferred, &promise);
  REJECT_RETURN;

	size_t argc = 1;
	napi_value argv[1];
	c->status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
	REJECT_RETURN;

	if (argc < 1) {
		REJECT_ERROR_RETURN("Get server list must be provided with a IOR reference to an ISA server.",
	    QGW_INVALID_ARGS);
	}
	c->status = napi_get_value_string_utf8(env, argv[0], nullptr, 0, &iorLen);
	REJECT_RETURN;
	isaIOR = (char*) malloc((iorLen + 1) * sizeof(char));
	c->status = napi_get_value_string_utf8(env, argv[0], isaIOR, iorLen + 1, &iorLen);
	REJECT_RETURN;

	c->isaIOR = isaIOR;

	c->status = napi_create_string_utf8(env, "GetServers", NAPI_AUTO_LENGTH, &resourceName);
  REJECT_RETURN;
  c->status = napi_create_async_work(env, nullptr, resourceName, getServersExecute,
    getServersComplete, c, &c->_request);
  REJECT_RETURN;
  c->status = napi_queue_async_work(env, c->_request);
  REJECT_RETURN;

  return promise;
}