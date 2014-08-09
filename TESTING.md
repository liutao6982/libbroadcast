# Libbroadcast testing server installation

FIXME: Write instructions on how to set up the CentOS server.

These instructions are for the configuration of a Linux server based on the Mishira CentOS base system for use as a network testing server for Libbroadcast. Developers can use this server to test RTMP and other network related code either manually or through unit tests. The IP of this server should be `192.168.1.151`.

These instructions assume a clean base system.

## Firewall testing

In order to test our connections against invalid ports we need to configure IPTables with the following settings:

```
# LAST_RULE=`iptables -L INPUT --line-numbers | awk 'END{print $1}'`
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 1936 -j ACCEPT
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 1937 -j REJECT --reject-with tcp-reset
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 1938 -j REJECT --reject-with icmp-host-prohibited
# service iptables save
```

**Port 1936** can then be used to test against open but non-listening ports, **port 1937** can be used to test against firewalls that issue a TCP RST packet and **port 1938** can be used to test against firewalls that issue an ICMP "host prohibited" packet.

## nginx + nginx-rtmp-module

In order to install the RTMP module for nginx we need to compile it from source. The procedure below installs nginx in a way that is compatible with CentOS but not identical to the [official CentOS packages](nginx.org/en/linux_packages.html). **Do not install the official package or any other web server on the same system** as this installation is not compatible with them.

Version numbers to install:

- nginx 1.5.3 (644a07952629)
- nginx-rtmp-module (a3cffbb6c213d96a505cb35b1b14975b68e384f9)
- OpenSSL 1.0.1e

**Login as an administrator** and **make a working directory** for the following steps with:

```
$ mkdir ~/nginx
$ cd ~/nginx
```

**Install the dependencies** from the CentOS repository with:

```
$ sudo yum install git gcc make
```

**Download the nginx source code** of the version specified above from the [official repository](http://hg.nginx.org/nginx/) and extract it with:

```
$ wget http://hg.nginx.org/nginx/archive/644a07952629.tar.gz
$ tar -xzvf 644a07952629.tar.gz
$ rm 644a07952629.tar.gz
```

**Clone the `nginx-rtmp-module` Git repository** and check out the commit specified above with:

```
$ git clone https://github.com/arut/nginx-rtmp-module.git
$ cd nginx-rtmp-module
$ git checkout a3cffbb
$ cd ..
```

**Download the OpenSSL source code** of the version specified above from the [official website](http://www.openssl.org/source/) and extract it with:

```
$ wget http://www.openssl.org/source/openssl-1.0.1e.tar.gz
$ tar -xzvf openssl-1.0.1e.tar.gz
$ rm openssl-1.0.1e.tar.gz
```

**Build nginx** and customise its installation with:

```
$ cd nginx-644a07952629
$ auto/configure --prefix=/etc/nginx --sbin-path=/usr/sbin/nginx --conf-path=/etc/nginx/nginx.conf --error-log-path=/var/log/nginx/error.log --http-log-path=/var/log/nginx/access.log --pid-path=/var/run/nginx.pid --lock-path=/var/run/nginx.lock --http-client-body-temp-path=/var/cache/nginx/client_temp --http-proxy-temp-path=/var/cache/nginx/proxy_temp --http-fastcgi-temp-path=/var/cache/nginx/fastcgi_temp --http-uwsgi-temp-path=/var/cache/nginx/uwsgi_temp --http-scgi-temp-path=/var/cache/nginx/scgi_temp --user=nginx --group=nginx --add-module=../nginx-rtmp-module --with-http_ssl_module --without-http_gzip_module --without-http_rewrite_module --with-openssl=../openssl-1.0.1e
$ make && DESTDIR=../build make install
$ cd ../build
$ mkdir -p ./var/www
$ mv ./etc/nginx/html ./var/www/
$ mkdir -p ./var/cache/nginx
$ rm ./etc/nginx/*.default
$ rmdir ./var/run
$ mkdir -p ./etc/logrotate.d
$ mkdir -p ./etc/rc.d/init.d
$ mkdir -p ./etc/sysconfig
```

**Create** `./etc/logrotate.d/nginx` with the following contents (Based off the CentOS Apache package):

```
/var/log/nginx/*.log {
	missingok
	notifempty
	sharedscripts
	delaycompress
	postrotate
		/sbin/service nginx reload > /dev/null 2>/dev/null || true
	endscript
}
```

**Create** `./etc/rc.d/init.d/nginx` with the following contents (Based off the CentOS nginx package):

```
#!/bin/sh
#
# nginx        Startup script for nginx
#
# chkconfig: - 85 15
# processname: nginx
# config: /etc/nginx/nginx.conf
# config: /etc/sysconfig/nginx
# pidfile: /var/run/nginx.pid
# description: nginx is an HTTP and reverse proxy server
#
### BEGIN INIT INFO
# Provides: nginx
# Required-Start: $local_fs $remote_fs $network
# Required-Stop: $local_fs $remote_fs $network
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: start and stop nginx HTTP server
### END INIT INFO

# Source function library.
. /etc/rc.d/init.d/functions

if [ -f /etc/sysconfig/nginx ]; then
	. /etc/sysconfig/nginx
fi

prog=nginx
nginx=${NGINX-/usr/sbin/nginx}
conffile=${CONFFILE-/etc/nginx/nginx.conf}
lockfile=${LOCKFILE-/var/lock/subsys/nginx}
pidfile=${PIDFILE-/var/run/nginx.pid}
SLEEPMSEC=100000
RETVAL=0

start() {
	echo -n $"Starting $prog: "

	daemon --pidfile=${pidfile} ${nginx} -c ${conffile}
	RETVAL=$?
	echo
	[ $RETVAL = 0 ] && touch ${lockfile}
	return $RETVAL
}

stop() {
	echo -n $"Stopping $prog: "
	killproc -p ${pidfile} ${prog}
	RETVAL=$?
	echo
	[ $RETVAL = 0 ] && rm -f ${lockfile} ${pidfile}
}

reload() {
	echo -n $"Reloading $prog: "
	killproc -p ${pidfile} ${prog} -HUP
	RETVAL=$?
	echo
}

upgrade() {
	oldbinpidfile=${pidfile}.oldbin

	configtest -q || return 6
	echo -n $"Staring new master $prog: "
	killproc -p ${pidfile} ${prog} -USR2
	RETVAL=$?
	echo
	/bin/usleep $SLEEPMSEC
	if [ -f ${oldbinpidfile} -a -f ${pidfile} ]; then
		echo -n $"Graceful shutdown of old $prog: "
		killproc -p ${oldbinpidfile} ${prog} -QUIT
		RETVAL=$?
		echo
	else
		echo $"Upgrade failed!"
		return 1
	fi
}

configtest() {
	if [ "$#" -ne 0 ] ; then
		case "$1" in
			-q)
				FLAG=$1
				;;
			*)
				;;
		esac
		shift
	fi
	${nginx} -t -c ${conffile} $FLAG
	RETVAL=$?
	return $RETVAL
}

rh_status() {
	status -p ${pidfile} ${nginx}
}

# See how we were called.
case "$1" in
	start)
		rh_status >/dev/null 2>&1 && exit 0
		start
		;;
	stop)
		stop
		;;
	status)
		rh_status
		RETVAL=$?
		;;
	restart)
		configtest -q || exit $RETVAL
		stop
		start
		;;
	upgrade)
		upgrade
		;;
	condrestart|try-restart)
		if rh_status >/dev/null 2>&1; then
			stop
			start
		fi
		;;
	force-reload|reload)
		reload
		;;
	configtest)
		configtest
		;;
	*)
		echo $"Usage: $prog {start|stop|restart|condrestart|try-restart|force-reload|upgrade|reload|status|help|configtest}"
		RETVAL=2
esac

exit $RETVAL
```

**Create** `./etc/sysconfig/nginx` with the following contents (Based off the CentOS nginx package):

```
# Configuration file for the nginx service.

NGINX=/usr/sbin/nginx
CONFFILE=/etc/nginx/nginx.conf
```

**Replace the contents** of `./etc/nginx/nginx.conf` with the following:

```
user                nginx;
worker_processes    1;

error_log  /var/log/nginx/error.log warn;
pid        /var/run/nginx.pid;

events {
	worker_connections  1024;
}

http {
	include         /etc/nginx/mime.types;

	# Use the standardised "combined" log format
	log_format      main
		'$remote_addr - $remote_user [$time_local] "$request" '
		'$status $body_bytes_sent "$http_referer" '
		'"$http_user_agent"';

	access_log          /var/log/nginx/access.log   main;
	sendfile            on;
	#tcp_nopush         on;
	keepalive_timeout   65;

	server {
		listen      8080;
		server_name localhost;

		location / {
			root    /var/www/html;
			index   index.html index.htm;
		}

		#error_page 404 /404.html;

		# Redirect server error pages to the static page /50x.html
		error_page  500 502 503 504 /50x.html;
		location = /50x.html {
		    root    /var/www/html;
		}
	}

}

rtmp {
	server {
		listen 1935;

		ping 10s;
		ping_timeout 10s;
		ack_window 2500000;

		application testApp {
			allow publish all;
			allow play all;

			live on;
			record off;

			publish_notify on;

			hls on;
			hls_path /tmp/hls;
			hls_fragment 5s;
		}
	}
}
```

**Finalise the customised installation** with:

```
$ chmod +x ./etc/rc.d/init.d/nginx
$ chcon -R -u system_u -t etc_t ./etc
$ chcon -R -u system_u -t initrc_exec_t ./etc/rc.d/init.d/nginx
$ chcon -R -u system_u -t usr_t ./usr
$ chcon -R -u system_u -t bin_t ./usr/sbin
$ chcon -R -u system_u -t var_t ./var
$ chcon -R -u system_u -t var_log_t ./var/log
$ chcon -R -u system_u -t public_content_t ./var/www
```

**Install nginx** to the system with:

```
$ sudo chown -R root:root ./*
$ tar -cp --selinux -f ../build.tar ./*
$ cd ..
$ sudo rm -rf ./build
$ sudo tar -xf ./build.tar -C /
$ sudo -i
# useradd -Ur -s /bin/nologin -d /var/www -c "nginx" nginx
# chown nginx:nginx /var/cache/nginx
# chmod 700 /var/cache/nginx
# chkconfig --add nginx
# service nginx start
```

**Open TCP ports** 1935 and 8080 in IPTables with:

```
# LAST_RULE=`iptables -L INPUT --line-numbers | awk 'END{print $1}'`
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 1935 -j ACCEPT
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 8080 -j ACCEPT
# service iptables save
```

nginx with the RTMP module has now been installed. All files in the `~/nginx` directory can now be deleted if required.

## Adobe Media Server 5 Starter (A.k.a. AMS and FMS)

**Download the Adobe Media Server 5 Starter edition** from the official Adobe website. The downloaded file should have the filename `AdobeMediaServer_5_LS1_linux64.tar.gz`. This step requires logging into the Adobe website.

**Copy the file** to the server over SCP to a directory called `~/ams` on an administrator's account.

**Login as the administrator** and navigate to the `~/ams` directory.

**Extract the installer** and enter its directory with the following. Note that the version number might be different than what is shown below.

```
$ tar -xzvf AdobeMediaServer_5_LS1_linux64.tar.gz
$ rm AdobeMediaServer_5_LS1_linux64.tar.gz
$ cd AMS_5_0_3_r3029
```

**Execute the installer** with:

```
$ sudo ./installAMS
```

Follow the on-screen instructions. When asked to enter a serial number just hit enter and it will install the free Starter edition. Install to the default directory (`/opt/adobe/ams`) and use `admin` for the administrator username and `asdfasdf` as the password. We want the server to run as the default `ams` user and group. **Do install Apache**, let it listen on port 80 and run it as the `ams` user as it is needed to access the sample pages. Use ports 1934 for streaming instead of the default as we already have another RTMP server running on port 1935 and use the default port (1111) for the administration service. Let the server run as a daemon and have it automatically start after installation.

**Open TCP ports** 80, 1111 and 1934 in IPTables with:

```
$ sudo -i
# LAST_RULE=`iptables -L INPUT --line-numbers | awk 'END{print $1}'`
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 80 -j ACCEPT
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 1111 -j ACCEPT
# iptables -I INPUT $LAST_RULE -m state --state NEW -m tcp -p tcp --dport 1934 -j ACCEPT
# service iptables save
```

Adobe Media Server has now been installed. All files in the `~/ams` directory can now be deleted if required.
