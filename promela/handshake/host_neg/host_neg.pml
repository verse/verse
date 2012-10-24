/**
 * $Id: host_neg.pml 709 2010-11-26 14:31:37Z jiri $
 *
 * This PROMELA program simulates and helps to verify the negotiation of new
 * datagram connection between Verse client and Verse server.
 *
 * author: Jiri Hnidek <jiri.hnidek@tul.cz>
 */

mtype = {none, change_l, change_r, confirm_l, confirm_r}

#define NONE_URL	0
#define VALID_URL	1
#define UNVALID_URL	2

#define UNSTABLE	0
#define STABLE		1

/**
 * This process simulates the Verse client during negotiation of new datagram
 * connection.
 */
proctype Client(chan in; chan out)
{
	mtype cmd1, cmd2;
	byte url1, url2;
	bit stable;
	
	/* This client could be stable or potentionaly unstable */
	do
	:: stable = STABLE -> { printf("Client: stable\n"); break; };
	:: stable = UNSTABLE -> { printf("Client: unstable\n"); break; };
	od;
	
authenticated:
	printf("Client: AUTHENTICATED\n");
	
	do
	:: (1) -> break;
	:: (stable==UNSTABLE) -> { goto end_failure; };
	od;

propose_url:
	printf("Client: PROPOSE_URL\n");
	out!change_r, UNVALID_URL, none, 0;
	
	/* Wait for servere response */
	do
	:: in?cmd1, url1, cmd2, url2;
		if
		:: (cmd1==confirm_r &&
			url1==NONE_URL &&
			cmd2==change_l &&
			url2==VALID_URL) -> { goto try_url; };
		:: timeout -> { goto end_failure; };
		fi;
	:: (stable==UNSTABLE) -> { goto end_failure; };
	:: timeout -> { goto end_failure; };
	od;	
	
try_url:
	printf("Client: TRY_URL\n");
	/* Random pick result of connection */
	do
	:: url1=VALID_URL ->
		{ printf("Client: Dgram handshake successful\n"); break; };
	:: url1=NONE_URL ->
		{ printf("Client: Dgram handshake failed\n"); break; };
	od;
	
	do
	:: (1) -> break;
	:: (stable==UNSTABLE) -> { goto end_failure; };
	od;

negotiate_url:
	printf("Client: NEGOTIATE_URL\n");
	out!confirm_l, url1, none, 0;
	if
	:: (url1==VALID_URL) -> { goto end_success; };
	:: (url1==NONE_URL) -> { goto end_failure; };
	fi;

end_failure:
	printf("Client: FAILURE\n");
	
	/* Wait for other proccess */
	do
	:: in?cmd1, url1, cmd2, url2;
	:: timeout -> { goto end; };
	od;
	
end_success:
	printf("Client: SUCCESS\n");

end:
	skip;
}

	

/**
 * This process simulates the Verse server during negotiation of new
 * datagram connection.
 */
proctype Server(chan in; chan out)
{
	mtype cmd1, cmd2;
	byte url1, url2;
	bit stable;

	/* This server could be stable or potentially unstable */
	do
	:: stable = STABLE -> { printf("Server: stable\n"); break; };
	:: stable = UNSTABLE -> { printf("Server: unstable\n"); break; };
	od;
	
authenticated:
	printf("Server: AUTHENTICATED\n");

	/* Wait for URL proposal */
	do
	:: in?cmd1, url1, cmd2, url2;
		if
		:: (cmd1==change_r &&
			url1==UNVALID_URL) -> { goto create_url; };
		:: else  -> { goto end_failure; };
		fi;
	:: (stable==UNSTABLE) -> { goto end_failure; };
	:: timeout -> { goto end_failure; };
	od;

create_url:
	printf("Server: CREATE_URL\n");
	
	/* Random pick result of creating new dgram socket */
	do
	:: url2=VALID_URL ->
		{ printf("Server: Open dgram port successful\n"); break; };
	:: url2=NONE_URL -> 
		{ printf("Server: Open dgram port failed\n"); break; };
	od;

	out!confirm_r, NONE_URL, change_l, url2;
	if
	:: (url2==NONE_URL) -> { goto end_failure; };
	:: else -> skip;
	fi;

negotiate_url:
	/* Wait for URL confirmation */
	do
	:: in?cmd1, url1, cmd2, url2;
		if
		:: (cmd1==confirm_l &&
			url1==VALID_URL) -> { goto end_success; };
		:: else -> { goto end_failure; };
		fi;
	:: (stable==UNSTABLE) -> { goto end_failure; };
	:: timeout -> { goto end_failure; };
	od;

end_failure:
	printf("Server: FAILURE\n");
	
	/* Wait for other proccess */
	do
	:: in?cmd1, url1, cmd2, url2;
	:: timeout -> { goto end; };
	od;
	
end_success:
	printf("Server: SUCCESS\n");

end:
	skip;
}

/**
 * Initial process, where communication channels are created and client and
 * server processes are started. Channels have zero length. It means that
 * channels are synchronous.
 */
init
{
	/* cmd_type, URL, cmd_type, URL */
	chan q1 = [0] of {mtype, byte, mtype, byte};
	/* cmd_type, URL, cmd_type, URL */
	chan q2 = [0] of {mtype, byte, mtype, byte};
	
	atomic{run Server(q1, q2); run Client(q2, q1);};
}