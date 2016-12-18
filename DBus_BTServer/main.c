#ifdef __cplusplus
extern "C" {
#endif

#include <gio/gio.h>
#include <stdlib.h>
#include <gio/gunixfdlist.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/rfkill.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <fcntl.h>
#include <string.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <obexftp/client.h>


/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

GMainLoop *loop;
GDBusConnection *connection;

const char* get_file_version()
{
    return "version_1";
}

void turn_bt_on()
{
    printf("Turning bluetooth on \n");
    struct rfkill_event rd_event = {0};
    int rfkill_fd;
    if((rfkill_fd = open("/dev/rfkill", O_RDWR)) < 0){
       perror("open");
    }
    fcntl(rfkill_fd, F_SETFL, O_NONBLOCK);
    struct rfkill_event w_event = {0};
    w_event.type = RFKILL_TYPE_BLUETOOTH; /* Targetting Bluetooth switches */
    w_event.op = RFKILL_OP_CHANGE_ALL; /* Change all switches */
    w_event.soft = 0; /* Set to block */
    if(write(rfkill_fd, &w_event, sizeof(w_event)) < 0){
        perror("write");
    }
    while(read(rfkill_fd, &rd_event, sizeof(rd_event)) > 0){
        if(rd_event.type == RFKILL_TYPE_BLUETOOTH)
            printf("idx: %d, Bluetooth chip switches: soft(%d), hard(%d).\n",
                   rd_event.idx, rd_event.soft, rd_event.hard);
    }
    close(rfkill_fd);
    sleep(1);
}
struct handler_data {
  const char *bt_address;
  short rssi;
};

void send_file(const char * filename)
{
    const char *device = "E4:F8:EF:A8:A4:4C";
    int channel = -1;
    obexftp_client_t *cli = NULL; /*!!!*/
    int ret;

    channel = obexftp_browse_bt_push(device); /*!!!*/

    /* Open connection */
    cli = obexftp_open(OBEX_TRANS_BLUETOOTH, NULL, NULL, NULL); /*!!!*/
    if (cli == NULL)
    {
        fprintf(stderr, "Error opening obexftp client\n");
        exit(1);
    }

    /* Connect to device */
    ret = obexftp_connect_push(cli, device, channel); /*!!!*/
    if (ret < 0)
    {
        fprintf(stderr, "Error connecting to obexftp device\n");
        obexftp_close(cli);
        cli = NULL;
        exit(1);
    }

    /* Push file */
    ret = obexftp_put_file(cli, filename, filename); /*!!!*/
    if (ret < 0)
    {
        fprintf(stderr, "Error putting file\n");
    }

    /* Disconnect */
    ret = obexftp_disconnect(cli); /*!!!*/
    if (ret < 0)
    {
        fprintf(stderr, "Error disconnecting the client\n");
    }

    /* Close */
    obexftp_close(cli); /*!!!*/
    cli = NULL;
}

sdp_session_t *register_service(uint8_t rfcomm_channel)
{
    uint32_t svc_uuid_int[] = { 0, 0, 0, 0xABCD }; //for convenience
    uint8_t rfcomm_port = 11;
    const char *service_name = "Ugly Service to be advertised ";
    const char *service_dsc = "Temporary solution to be changed with d-bus ";
    const char *service_prov = "Michal Walczak is only provider and only user ";
    uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid, svc_class_uuid;
    sdp_list_t *l2cap_list = 0,
    *rfcomm_list = 0,
    *root_list = 0,
    *proto_list = 0,
    *access_proto_list = 0,
    *svc_class_list = 0,
    *profile_list = 0;
    sdp_data_t *channel = 0;
    sdp_profile_desc_t profile;
    sdp_record_t record = { 0 };
    sdp_session_t *session = 0;
    // PART ONE
    // set the general service ID
    sdp_uuid128_create( &svc_uuid, &svc_uuid_int );
    sdp_set_service_id( &record, svc_uuid );
    // set the service class
    sdp_uuid16_create(&svc_class_uuid, SERIAL_PORT_SVCLASS_ID);
    svc_class_list = sdp_list_append(0, &svc_class_uuid);
    sdp_set_service_classes(&record, svc_class_list);
    // set the Bluetooth profile information
    sdp_uuid16_create(&profile.uuid, SERIAL_PORT_PROFILE_ID);
    profile.version = 0x0100;
    profile_list = sdp_list_append(0, &profile);
    sdp_set_profile_descs(&record, profile_list);
    // make the service record publicly browsable
    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups( &record, root_list );
    // set l2cap information
    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    l2cap_list = sdp_list_append( 0, &l2cap_uuid );
    proto_list = sdp_list_append( 0, l2cap_list );
    // register the RFCOMM channel for RFCOMM sockets
    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append( 0, &rfcomm_uuid );
    sdp_list_append( rfcomm_list, channel );
    sdp_list_append( proto_list, rfcomm_list );
    access_proto_list = sdp_list_append( 0, proto_list );
    sdp_set_access_protos( &record, access_proto_list );
    // set the name, provider, and description
    sdp_set_info_attr(&record, service_name, service_prov, service_dsc);
    // PART TWO
    // connect to the local SDP server, register the service record, and
    // disconnect
    bdaddr_t tmp_bdaddr_local = (bdaddr_t) {{0, 0, 0, 0xff, 0xff, 0xff}};
    bdaddr_t tmp_bdaddr = (bdaddr_t) {{0, 0, 0, 0, 0, 0}};
    session = sdp_connect( &tmp_bdaddr, &tmp_bdaddr_local, 0 );
    sdp_record_register(session, &record, 0);
    // cleanup
    sdp_data_free( channel );
    sdp_list_free( l2cap_list, 0 );
    sdp_list_free( rfcomm_list, 0 );
    sdp_list_free( root_list, 0 );
    sdp_list_free( access_proto_list, 0 );
    return session;
}

void start_server()
{
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int s, client, bytes_read, port, status;
    socklen_t opt = sizeof(rem_addr);

    // allocate socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // bind socket to port 1 of the first available
    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = (bdaddr_t) {{0, 0, 0, 0, 0, 0}};
    for( port = 1; port <= 30; port++ ) {
    loc_addr.rc_channel = port;
    status = bind(s, (struct sockaddr *)&loc_addr, sizeof( loc_addr ) );
    if( status == 0 )
        {
            printf("listen on port: [%d]\n", loc_addr.rc_channel);
            break;
        }
    }
    // check sdptool browse local for checking
    sdp_session_t* session = register_service(loc_addr.rc_channel);

    // put socket into listening mode
    listen(s, loc_addr.rc_channel);

    // accept one connection
    client = accept(s, (struct sockaddr *)&rem_addr, &opt);

    ba2str( &rem_addr.rc_bdaddr, buf );
    fprintf(stderr, "accepted connection from %s\n", buf);
    write(client, "Welcome", sizeof("Welcome"));
    memset(buf, 0, sizeof(buf));
    char msg[1024];
    int bytes_send = 0;
    while(buf[0] != 'q')
    {
        // read data from the client
        bytes_read = read(client, buf, sizeof(buf));
        if( bytes_read > 0 )
        {
            printf("received [%s]\n", buf);
        }
        //copy buf and prepend echo
        char sending_buf[1024] = { 0 };
        memcpy(sending_buf, buf, sizeof(buf));
        const char* echo = "echo ";
        size_t len = strlen(echo);
        size_t i;
        memmove(sending_buf + len, sending_buf, strlen(sending_buf) + 1);
        for (i = 0; i < len; ++i)
        {
            sending_buf[i] = echo[i];
        }
        //echo data bacck to the client
        bytes_send = write(client, sending_buf, sizeof(sending_buf));
        if( bytes_send > 0 )
        {
            printf("send [%s]\n", sending_buf);
        }
        memset(sending_buf, 0, sizeof(sending_buf));
    }
    // close connection
    sdp_close( session );
    close(client);
    close(s);
}

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.gtk.GDBus.TestInterface'>"
"    <method name='GetVersion'>"
"      <annotation name='org.gtk.GDBus.Annotation' value='OnMethod'/>"
"      <arg type='s' name='in' direction='in'/>"
"      <arg type='s' name='out' direction='out'/>"
"    </method>"
"    <method name='StartServer'>"
"      <annotation name='org.gtk.GDBus.Annotation' value='OnMethod'/>"
"      <arg type='s' name='in' direction='in'/>"
"      <arg type='s' name='out' direction='out'/>"
"    </method>"
"    <method name='SendFile'>"
"      <annotation name='org.gtk.GDBus.Annotation' value='OnMethod'/>"
"      <arg type='s' name='in' direction='in'/>"
"      <arg type='s' name='out' direction='out'/>"
"    </method>"
"    <property type='s' name='very_important_file_version' access='read'>"
"      <annotation name='org.gtk.GDBus.Annotation' value='OnProperty'>"
"        <annotation name='org.gtk.GDBus.Annotation' value='OnAnnotation_YesThisIsCrazy'/>"
"      </annotation>"
"    </property>"
"  </interface>"
"</node>";

/* ---------------------------------------------------------------------------------------------------- */

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  if (g_strcmp0 (method_name, "GetVersion") == 0)
      {
        GError *local_error;
        const char *version = get_file_version();

        gchar *response;
        response = g_strdup_printf ("Current version is: %s", version);
        g_dbus_method_invocation_return_value (invocation,
                                               g_variant_new ("(s)", response));
        g_free (response);
      }
  else if (g_strcmp0 (method_name, "StartServer") == 0)
  {
      GError *local_error;
      turn_bt_on(); //ugly rfkill option
      gchar *response;
      response = g_strdup_printf ("starting server");
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", response));
      g_free (response);
      start_server();
  }
  else if (g_strcmp0 (method_name, "SendFile") == 0)
  {
      char * filename;
      GError *local_error;
      g_variant_get (parameters, "(&s)", &filename);
      gchar *response;
      response = g_strdup_printf ("sending file...");
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", response));
      g_free (response);
      send_file(filename); //ugly obexftp option
  }
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
  GVariant *ret;

  ret = NULL;
  if (g_strcmp0 (property_name, "very_important_file_version") == 0)
    {
      ret = g_variant_new_string (get_file_version());
    }
  return ret;
}

/* for now */
static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  handle_get_property,
};

/* ---------------------------------------------------------------------------------------------------- */

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  guint registration_id;

  registration_id = g_dbus_connection_register_object (connection,
                                               "/org/gtk/GDBus/TestObject",
                                               introspection_data->interfaces[0],
                                               &interface_vtable,
                                               NULL,  /* user_data */
                                               NULL,  /* user_data_free_func */
                                               NULL); /* GError** */
  g_assert (registration_id > 0);
}

static void
on_name_acquired (GDBusConnection *connection, const gchar *name,
                  gpointer user_data)
{
}

static void on_name_lost (GDBusConnection *connection, const gchar *name,
                          gpointer user_data)
{
  exit (1);
}

//static void
//device_found_handler (GDBusConnection *connection,
//                        const gchar *sender_name,
//                        const gchar *object_path,
//                        const gchar *interface_name,
//                        const gchar *signal_name,
//                        GVariant *parameters,
//                        gpointer user_data)
//{
//  char *device_address;
//  gboolean res;
//  short rssi;
//  GVariant *property_dict;
//  struct handler_data *data = (struct handler_data *)user_data;
//
//  /*
//   * Paramter format: sa{sv}
//   * Only interested in the RSSI so lookup that entry in the properties
//   * dictionary.
//   */
//  g_variant_get(parameters, "(&s*)", &device_address, &property_dict);
//
//  res = g_variant_lookup(property_dict, "RSSI", "n",
//             &rssi);
//  if (!res) {
//    printf("Unable to get device address from dbus\n");
//    g_main_loop_quit(loop);
//    return;
//  }
//
//  data->rssi = rssi;
//  g_main_loop_quit(loop);
//}

int
main (int argc, char *argv[])
{
  guint owner_id;
  GDBusMessage *method_call_message;
  GDBusMessage *method_reply_message;
  char *adapter_object = NULL;
  GError *local_error = NULL;
  GVariant *reply;
  GVariant **t = g_new(GVariant *, 2);
  t[0] = g_variant_new_string("Powered");
  t[1] = g_variant_new_variant(g_variant_new_boolean(TRUE));
  GVariant *bt_off_variant = g_variant_new_tuple(t, 2);
  const char **value = NULL;
  struct handler_data data;
  g_free(t);

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &local_error);
  /* We are lazy here - we don't want to manually provide
   * the introspection data structures - so we just build
   * them from XML.
   */
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);
  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.TestServer",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  GVariant **p = g_new(GVariant *, 1);
  t[0] = g_variant_new_string("hci0");
  GVariant *pattern = g_variant_new_tuple(t, 1);
  reply = g_dbus_connection_call_sync(connection,
                      "org.bluez",
                      "/",
                      "org.bluez.Manager",
                      "FindAdapter",
                      pattern,
                      G_VARIANT_TYPE("(o)"),
                      G_DBUS_CALL_FLAGS_NONE,
                      -1,
                      NULL,
                      &local_error);

  if (local_error) {
    printf("Unable to get managed objects: %s\n", local_error->message);
    return 0;
  }
  g_variant_get(reply, "(&o)", &adapter_object);
  printf("Found objects: %s\n", adapter_object);

//  /* Register a handler for DeviceFound signals to read the device RSSI */
//  g_dbus_connection_signal_subscribe(connection,
//                     NULL,
//                     "org.bluez.Adapter",
//                     "DeviceFound",
//                     NULL,
//                     NULL,
//                     G_DBUS_SIGNAL_FLAGS_NONE,
//                     device_found_handler,
//                     &data,
//                     NULL);

  /* Start device discovery */
  reply = g_dbus_connection_call_sync(connection,
                     "org.bluez",
                     adapter_object,
                     "org.bluez.Adapter",
                     "StartDiscovery",
                     NULL,
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     &local_error);

  if (local_error) {
    printf("Unable to start discovery: %s\n", local_error->message);
    return 0;
  }

  g_variant_unref (reply);
  g_bus_unown_name (owner_id);
  g_dbus_node_info_unref (introspection_data);

  return 0;
}
#ifdef __cplusplus
}
#endif
