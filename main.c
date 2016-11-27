#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/rfkill.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int main(int argc, char **argv)
{
    int rfkill_fd;
    if((rfkill_fd = open("/dev/rfkill", O_RDWR)) < 0){
       perror("open");
       return(1);
    }
    fcntl(rfkill_fd, F_SETFL, O_NONBLOCK);
    struct rfkill_event rd_event = {0};
    while(read(rfkill_fd, &rd_event, sizeof(rd_event)) > 0){
        if(rd_event.type == RFKILL_TYPE_BLUETOOTH)
            printf("idx: %d, Bluetooth chip switches: soft(%d), hard(%d).\n",
                   rd_event.idx, rd_event.soft, rd_event.hard);
    }
    struct rfkill_event w_event = {0};
    w_event.type = RFKILL_TYPE_BLUETOOTH; /* Targetting Bluetooth switches */
    w_event.op = RFKILL_OP_CHANGE_ALL; /* Change all switches */
    if (rd_event.soft)
    {
        w_event.soft = 0; /* Set to unblock */
    }
    else
    {
        w_event.soft = 1; /* Set to block */
    }
    if(write(rfkill_fd, &w_event, sizeof(w_event)) < 0){
        perror("write");
        return(1);
    }

    while(read(rfkill_fd, &rd_event, sizeof(rd_event)) > 0){
        if(rd_event.type == RFKILL_TYPE_BLUETOOTH)
            printf("idx: %d, Bluetooth chip switches: soft(%d), hard(%d).\n",
                   rd_event.idx, rd_event.soft, rd_event.hard);
    }

    return(1);
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int s, client, bytes_read;
    socklen_t opt = sizeof(rem_addr);

    // allocate socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // bind socket to port 1 of the first available
    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 22;
    bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));

    // put socket into listening mode
    listen(s, 22);

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
    close(client);
    close(s);
    close(rfkill_fd);
    return 0;
}
