#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>

#include <time.h>
#define FALSE 0
#define TRUE 1

long BAUD = B9600;
long DATABITS = CS8;
long STOPBITS = 1;
long PARITYON = 0;
long PARITY = 0;

volatile int STOP=FALSE;

void signal_handler_IO (int status);    //definition of signal handler
int wait_flag=TRUE;                     //TRUE while no signal received
pid_t daemon_pid;
pid_t pid;

void signal_handler(int signum)
{
	printf("\r\nCaught signal %d\r\n",signum);
	exit(signum);
}

int main(int argc, char *argv[])
{

	char version1[80] = "WiseGears UPS Monitor Daemon for APC/Microsol";
	char line[80]     = "---------------------------------------------";
	char version2[80] = "Licensed under GNU Public License v3";
	char version3[80] = "Developed by WiseGears.com";
	char blank[80]    = "";
	char usage1[80]   = "Usage:";
	char usage2[80]   = " <device>";
	char usage3[80]   = "Examples:";
	char usage4[80]   = " /dev/ttyS0";
	char usage5[80]   = "\t(for a RS232 connected UPS)";
	char usage6[80]   = " /dev/ttyUSB0";
	char usage7[80]   = "\t(for a USB connected UPS)";
	char newline[80]   = "";

	char message[255];
	unsigned char buffer[255];
	unsigned char limbo[255];

	int fd, i, j, res,last_state;

	if (argc <= 1) {
		puts(version1);
		puts(line);
		puts(version2);
		puts(version3);
		puts(blank);

		puts(usage1);
		printf("\t%s",argv[0]);
		puts(usage2);
		puts(blank);
		puts(usage3);
		printf("\t%s",argv[0]);
		puts(usage4);
		puts(usage5);
		puts(blank);
		printf("\t%s",argv[0]);
		puts(usage6);
		puts(usage7);
		puts(blank);
	}
	else {
		daemon_pid = fork();
		if (daemon_pid > 0) {
			printf("WiseGears UPS Monitor daemon started and forked successfully.\r\n");
			exit(0);
		}
		signal(SIGINT,signal_handler);
		struct termios options;
   		struct sigaction saio;               //definition of signal action

		fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (fd < 0) {
			perror(argv[1]); //Null file descriptor, couldn't open
			exit(-1);        //device for some Microsoft reason
		}
		saio.sa_handler = signal_handler_IO;
		sigemptyset(&saio.sa_mask);
		saio.sa_flags = 0;
		saio.sa_restorer = NULL;
		sigaction(SIGIO,&saio,NULL);

		fcntl(fd, F_SETOWN, getpid());
		fcntl(fd, F_SETFL, FASYNC);

		tcgetattr(fd, &options);
		cfsetispeed(&options, B9600); //Sets IN  baud
		cfsetospeed(&options, B9600); //Sets OUT baud
		options.c_cflag = BAUD | CRTSCTS | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
		options.c_iflag = IGNPAR;
		options.c_oflag = 0;
		options.c_lflag = 0;       //ICANON;
		options.c_cc[VMIN]=1;
		options.c_cc[VTIME]=0;
		tcflush(fd, TCIFLUSH);

		tcsetattr(fd, TCSANOW, &options);
		j=0;
		last_state = 0;
		tcflush(fd, TCIFLUSH);
		tcflush(fd, TCIFLUSH);

		while (1) {
			if (wait_flag == FALSE) {
				res = read(fd,buffer,25);
				if (res > 0) {
					for (i=0; i<res; i++) {
						if (buffer[i] == 254) {
							j = 0;
#if defined(DEBUG)
							printf("\r\n");
							fflush(stdout);
#endif
							break;
						}
						j++;
						if (j == 21) {
							if (last_state == 0) {
								last_state = buffer[i];
							}
							if (last_state == buffer[i]) {
								// No state changed!
							} else {
								last_state = buffer[i];
#if defined(DEBUG)
								puts("\r\n* Email sent!");
#endif
								FILE *mailer = popen("/bin/mail -s 'UPS Monitoring' root@localhost", "w");
								fprintf(mailer, "UPS status changed!\r\n");
								fprintf(mailer, "\r\n");
								switch (buffer[i]) {      // STATUS  | Outlet State         | Battery State
										  // ------------------------------------------------
									case 11:  // ON      | outlet-power-present | charging
										fprintf(mailer, "Status       : On\r\n");
										fprintf(mailer, "Outlet power : Present\r\n");
										fprintf(mailer, "Battery      : Charging\r\n");
										FILE *fp1 = fopen("/usr/local/sbin/wgupsd_powerup.sh","r");
										if( fp1 ) {
										// exists
											fclose(fp1);
											pid = fork();
											if (pid == 0) {
												execve("/usr/local/sbin/wgupsd_powerup.sh &");
												exit(0);
											}
										} else {
											// doesnt exist
										}
										break;
									case 9:   // ON      | outlet-power-present | fully-charged
										fprintf(mailer, "Status       : On\r\n");
										fprintf(mailer, "Outlet power : Present\r\n");
										fprintf(mailer, "Battery      : Fully charged\r\n");
										break;
									case 43:  // ON      | no-outlet-power      | discharging
										fprintf(mailer, "Status       : On\r\n");
										fprintf(mailer, "Outlet power : UNAVAILABLE\r\n");
										fprintf(mailer, "Battery      : DISCHARGING\r\n");
										FILE *fp2 = fopen("/usr/local/sbin/wgupsd_powerdown.sh","r");
										if( fp2 ) {
										// exists
											fclose(fp2);

											pid = fork();
											if (pid == 0) {
												puts("Sou o child");
												execve("/usr/local/sbin/wgupsd_powerdown.sh &");
												exit(0);
											}
												puts("Sou o master");
										} else {
											// doesnt exist
										}
										break;
									case 3:   // OFF     | outlet-power-present | charging
										fprintf(mailer, "Status       : Off\r\n");
										fprintf(mailer, "Outlet power : Present\r\n");
										fprintf(mailer, "Battery      : Charging\r\n");
										break;
									case 1:   // OFF     | outlet-power-present | fully-charged
										fprintf(mailer, "Status       : Off\r\n");
										fprintf(mailer, "Outlet power : Present\r\n");
										fprintf(mailer, "Battery      : Fully charged\r\n");
										break;
									case 35:  // OFF     | no-outlet-power      | n/a
										fprintf(mailer, "Status       : Off\r\n");
										fprintf(mailer, "Outlet power : UNAVAILABLE\r\n");
										fprintf(mailer, "Battery      : UNKNOWN\r\n");
										break;
									case 99:  // SURT
										fprintf(mailer, "Status       : SURT DETECTED\r\n");
										fprintf(mailer, "Outlet power : UNKNOWN\r\n");
										fprintf(mailer, "Battery      : UNKNOWN\r\n");
										break;
									case 163: // OVERLOAD
										fprintf(mailer, "Status       : OVERLOAD\r\n");
										fprintf(mailer, "Outlet power : UNKNOWN\r\n");
										fprintf(mailer, "Battery      : UNKNOWN\r\n");
										break;
								}
								fprintf(mailer, "\r\n");
								fprintf(mailer, "\r\n");
								time_t curtime;
								struct tm *loctime;
								/* Get the current time. */
								curtime = time (NULL);
								/* Convert it to local time representation. */
								loctime = localtime (&curtime);
								strftime(message,sizeof(message),"%Y-%m-%d %H:%M:%S",loctime);

								fprintf(mailer, "-- email sent by WiseGears UPS Monitor at %s\r\n",message);
								fprintf(mailer, "\r\n");
								pclose(mailer);
							} //end-else
						}
						//sprintf(message,"%02x",buffer[i]);
#if defined(DEBUG)
						printf("%03d",buffer[i]);
						printf(" ");
						fflush(stdout);
#endif
						if (j >= 25)
						{
#if defined(DEBUG)
							printf("\r\n");
							fflush(stdout);
#endif
							j = 0;
						}
						//puts(message);
					}
					//puts(newline);
				}
			} //end if (wait_flag == FALSE)
		} //end-while
	}
}

void signal_handler_IO (int status)
{
//    printf("received SIGIO signal.\n");
       wait_flag = FALSE;
}
