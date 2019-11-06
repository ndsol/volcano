#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# gen_curl_config.py: configures libcurl for posix-style hosts,
# writing to the file specified on the command line (such as "curl_config.h")

import errno
import os
from select import select
import subprocess
import sys

class runcmd_stdin(object):
  def __init__(self, env):
    self.have_out = False
    self.have_err = False
    self.env = env
    # Add data to self.stdin before calling run():
    self.stdin = b""
    self.p = None

  def add_stdin(self, data):
    self.stdin += data
    return self

  def handle_output(self, is_stdout, out, data):
    out.write(data.decode('utf8', 'ignore')
        .encode(sys.stdout.encoding, 'replace').decode(sys.stdout.encoding))
    out.flush()

  def run(self, args):
    path_saved = os.environ["PATH"]
    if "PATH" in self.env:
      os.environ["PATH"] = self.env["PATH"]
    try:
      import pty
      masters, slaves = zip(pty.openpty(), pty.openpty())
      self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=slaves[0],
                                stderr=slaves[1], env = self.env)
      for fd in slaves: os.close(fd)
      writable = { self.p.stdin: self.p.stdin }
      readable = { masters[0]: sys.stdout, masters[1]: sys.stderr }
      while readable:
        rw = select(readable, writable, [])
        for fd in rw[1]:
          if fd == self.p.stdin:
            self.p.stdin.write(self.stdin)
            self.stdin = b""
            self.p.stdin.close()
            del writable[self.p.stdin]
        for fd in rw[0]:
          try: data = os.read(fd, 1024)
          except OSError as e:
            if e.errno != errno.EIO: raise
            del readable[fd]
            continue
          if not data:
            del readable[fd]
            continue
          self.handle_output(fd == masters[0], readable[fd], data)
          if fd != masters[0]:
            self.have_err = True
          else:
            self.have_out = True
      self.p.wait()
    except ImportError:
      if sys.platform != "win32":
        raise
      self.p = subprocess.Popen(args, shell=True)
      (o, e) = self.p.communicate()
      self.have_out = False
      self.have_err = False
      if o:
        self.have_out = True
        self.handle_output(True, sys.stdout, o)
      if e:
        self.have_err = True
        self.handle_output(False, sys.stdout, e)
    os.environ["PATH"] = path_saved

class runcmd_no_stderr(runcmd_stdin):
  def __init__(self, env):
    super(runcmd_no_stderr, self).__init__(env)
    self.stdout = b""

  def handle_output(self, is_stdout, out, data):
    if is_stdout:
      self.stdout = self.stdout + data
    else:
      super(runcmd_no_stderr, self).handle_output(is_stdout, out, data)

  def run(self, args):
    super(runcmd_no_stderr, self).run(args)
    if self.have_err or self.p.returncode != 0:
      print("%s failed" % args)
      sys.exit(1)

if len(sys.argv) != 5:
  print("Usage: %s prefix output.h current_os sizes.o" % sys.argv[0])
  sys.exit(1)

prefix = sys.argv[1]
current_os = sys.argv[3]
sizes_o = sys.argv[4]

def thru_gcc(c_code):
  # compile a test program, writing to og
  og = out + ".gen"
  cenv = os.environ.copy()
  r = runcmd_no_stderr(cenv).add_stdin(c_code)
  r.run([ 'gcc', '-Wall', '-Wextra', '-I%s' % prefix, '-xc', '-', '-o', og ])
  r = r.stdout.decode(sys.stdout.encoding)
  if r:
    print(r) # Show any gcc warnings
  r = runcmd_no_stderr(cenv)
  r.run([ og ])
  return r.stdout.decode(sys.stdout.encoding)

def get_MSG_NOSIGNAL():
  comment = ""
  if current_os == "mac":
    comment = "//"
  return comment + "#define HAVE_MSG_NOSIGNAL 1"

def get_CA_ROOT():
  return "/etc/ssl"

def get_CA_PATH():
  if current_os == "android":
    return "NULL"
  return "\"" + get_CA_ROOT() + "/certs\""

def get_CA_BUNDLE():
  if current_os == "mac":
    return "\"" + get_CA_ROOT() + "/cert.pem\""
  elif current_os == "android":
    return "NULL"
  return "\"" + get_CA_ROOT() + "/certs/ca-certificates.crt\""

def get_sizes():
  cenv = os.environ.copy()
  r = runcmd_no_stderr(cenv)
  r.run([ 'nm', sizes_o ])
  res = ""
  # Parse output of nm:
  # (hex)ofs type name
  # "00000100 C _SIZEOF_INT"
  # Note that _SIZEOF_INT is an array of 64 ints (to bypass alignment)
  for line in r.stdout.decode(sys.stdout.encoding).split("\n"):
    if line == "":
      break
    field = line.split(" ")
    assert len(field) == 3, "invalid nm output: \"%s\"" % line
    name = field[2].strip()
    if name.startswith("_"):
      name = name[1:]
    if name.startswith("SIZEOF_"):
      ofs = int(int(field[0], 16) / 64)
      res += "#define %s 0x%x\n" % (name, ofs)
  return res

# The os.makedirs ensures that any directories are created before output.
out = os.path.normpath(sys.argv[2])
try:
  os.makedirs(os.path.dirname(out))
except OSError as e:
  if e.errno != errno.EEXIST:
    raise

with open(out, "w") as fout:
  fout.write('''\
/* when building libcurl itself */
#define BUILDING_LIBCURL 1

/* Location of default ca bundle */
#define CURL_CA_BUNDLE {CURL_CA_BUNDLE}

/* define "1" to use built-in ca store of TLS backend */
#define CURL_CA_FALLBACK 1

/* Location of default ca path */
#define CURL_CA_PATH {CURL_CA_PATH}

/* to disable cookies support */
#define CURL_DISABLE_COOKIES 1

/* to disable cryptographic authentication */
#define CURL_DISABLE_CRYPTO_AUTH 1

/* to disable DICT */
#define CURL_DISABLE_DICT 1

/* to disable FILE */
#define CURL_DISABLE_FILE 1

/* to disable FTP */
#define CURL_DISABLE_FTP 1

/* to disable GOPHER */
#define CURL_DISABLE_GOPHER 1

/* to disable IMAP */
#define CURL_DISABLE_IMAP 1

/* to disable HTTP */
//#define CURL_DISABLE_HTTP 1

/* to disable LDAP */
#define CURL_DISABLE_LDAP 1

/* to disable LDAPS */
#define CURL_DISABLE_LDAPS 1

/* to disable POP3 */
#define CURL_DISABLE_POP3 1

/* to disable proxies */
#define CURL_DISABLE_PROXY 1

/* to disable RTSP */
#define CURL_DISABLE_RTSP 1

/* to disable SMB */
#define CURL_DISABLE_SMB 1

/* to disable SMTP */
#define CURL_DISABLE_SMTP 1

/* to disable TELNET */
#define CURL_DISABLE_TELNET 1

/* to disable TFTP */
#define CURL_DISABLE_TFTP 1

/* to disable verbose strings */
//#define CURL_DISABLE_VERBOSE_STRINGS 1

/* to make a symbol visible */
//#define CURL_EXTERN_SYMBOL $$CURL_EXTERN_SYMBOL
/* Ensure using CURL_EXTERN_SYMBOL is possible */
#ifndef CURL_EXTERN_SYMBOL
#define CURL_EXTERN_SYMBOL
#endif

/* Use Windows LDAP implementation */
//#define USE_WIN32_LDAP 1

/* when not building a shared library */
#define CURL_STATICLIB 1

/* your Entropy Gathering Daemon socket pathname */
#define EGD_SOCKET "EGD_SOCKET"

/* Define if you want to enable IPv6 support */
#define ENABLE_IPV6 1

/* Define to the type qualifier of arg 1 for getnameinfo. */
#define GETNAMEINFO_QUAL_ARG1 const struct sockaddr *

/* Define to the type of arg 1 for getnameinfo. */
#define GETNAMEINFO_TYPE_ARG1 struct sockaddr *

/* Define to the type of arg 2 for getnameinfo. */
#define GETNAMEINFO_TYPE_ARG2 socklen_t

/* Define to the type of args 4 and 6 for getnameinfo. */
#define GETNAMEINFO_TYPE_ARG46 socklen_t

/* Define to the type of arg 7 for getnameinfo. */
#define GETNAMEINFO_TYPE_ARG7 int

/* Specifies the number of arguments to getservbyport_r */
#define GETSERVBYPORT_R_ARGS 6

/* Specifies the size of the buffer to pass to getservbyport_r */
#define GETSERVBYPORT_R_BUFSIZE 4096

/* Define to 1 if you have the alarm function. */
#define HAVE_ALARM 1

/* Define to 1 if you have the <alloca.h> header file. */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the <arpa/tftp.h> header file. */
#define HAVE_ARPA_TFTP_H 1

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the `basename' function. */
#define HAVE_BASENAME 1

/* Define to 1 if bool is an available type. */
#define HAVE_BOOL_T 1

/* Define to 1 if you have the __builtin_available function. */
#define HAVE_BUILTIN_AVAILABLE 1

/* Define to 1 if you have the clock_gettime function and monotonic timer. */
#define HAVE_CLOCK_GETTIME_MONOTONIC 1

/* Define to 1 if you have the `closesocket' function. */
//#define HAVE_CLOSESOCKET 1
//#define HAVE_CLOSESOCKET_CAMEL 1
//#define HAVE_CLOSE_S 1
//#define USE_LWIPSOCK 1

/* Define to 1 if you have the `CRYPTO_cleanup_all_ex_data' function. */
#define HAVE_CRYPTO_CLEANUP_ALL_EX_DATA 1

/* Define to 1 if you have the <crypto.h> header file. */
#define HAVE_CRYPTO_H 1

/* Define to 1 if you have the <des.h> header file. */
#define HAVE_DES_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `ENGINE_load_builtin_engines' function. */
#define HAVE_ENGINE_LOAD_BUILTIN_ENGINES 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <err.h> header file. */
#define HAVE_ERR_H 1

/* Define to 1 if you have the fcntl function. */
#define HAVE_FCNTL 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have a working fcntl O_NONBLOCK function. */
#define HAVE_FCNTL_O_NONBLOCK 1

/* Define to 1 if you have the fdopen function. */
#define HAVE_FDOPEN 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the freeaddrinfo function. */
#define HAVE_FREEADDRINFO 1

/* Define to 1 if you have the freeifaddrs function. */
#define HAVE_FREEIFADDRS 1

/* Define to 1 if you have the ftruncate function. */
#define HAVE_FTRUNCATE 1

/* Define to 1 if you have a working getaddrinfo function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `geteuid' function. */
#define HAVE_GETEUID 1

/* Define to 1 if you have the gethostbyaddr function. */
#define HAVE_GETHOSTBYADDR 1

/* Define to 1 if you have the gethostbyaddr_r function. */
#define HAVE_GETHOSTBYADDR_R 1

/* gethostbyaddr_r() takes 5 args */
#define HAVE_GETHOSTBYADDR_R_5 0

/* gethostbyaddr_r() takes 7 args */
#define HAVE_GETHOSTBYADDR_R_7 0

/* gethostbyaddr_r() takes 8 args */
#define HAVE_GETHOSTBYADDR_R_8 1

/* Define to 1 if you have the gethostbyname function. */
#define HAVE_GETHOSTBYNAME 1

/* Define to 1 if you have the gethostbyname_r function. */
#define HAVE_GETHOSTBYNAME_R 1

/* gethostbyname_r() takes 3 args */
#define HAVE_GETHOSTBYNAME_R_3 0

/* gethostbyname_r() takes 5 args */
#define HAVE_GETHOSTBYNAME_R_5 0

/* gethostbyname_r() takes 6 args */
#define HAVE_GETHOSTBYNAME_R_6 1

/* Define to 1 if you have the gethostname function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have a working getifaddrs function. */
#define HAVE_GETIFADDRS 1

/* Define to 1 if you have the getnameinfo function. */
#define HAVE_GETNAMEINFO 1

/* Define to 1 if you have the `getpass_r' function. */
#define HAVE_GETPASS_R 1

/* Define to 1 if you have the `getppid' function. */
#define HAVE_GETPPID 1

/* Define to 1 if you have the `getprotobyname' function. */
#define HAVE_GETPROTOBYNAME 1

/* Define to 1 if you have the `getpeername' function. */
#define HAVE_GETPEERNAME 1

/* Define to 1 if you have the `getsockname' function. */
#define HAVE_GETSOCKNAME 1

/* Define to 1 if you have the `getpwuid' function. */
#define HAVE_GETPWUID 1

/* Define to 1 if you have the `getpwuid_r' function. */
#define HAVE_GETPWUID_R 1

/* Define to 1 if you have the `getrlimit' function. */
#define HAVE_GETRLIMIT 1

/* Define to 1 if you have the getservbyport_r function. */
#define HAVE_GETSERVBYPORT_R 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have a working glibc-style strerror_r function. */
//#define HAVE_GLIBC_STRERROR_R 1

/* Define to 1 if you have a working gmtime_r function. */
#define HAVE_GMTIME_R 1

/* if you have the gssapi libraries */
//#define HAVE_GSSAPI 1

/* Define to 1 if you have the <gssapi/gssapi_generic.h> header file. */
//#define HAVE_GSSAPI_GSSAPI_GENERIC_H 1

/* Define to 1 if you have the <gssapi/gssapi.h> header file. */
//#define HAVE_GSSAPI_GSSAPI_H 1

/* Define to 1 if you have the <gssapi/gssapi_krb5.h> header file. */
//#define HAVE_GSSAPI_GSSAPI_KRB5_H 1

/* if you have the GNU gssapi libraries */
//#define HAVE_GSSGNU 1

/* if you have the Heimdal gssapi libraries */
//#define HAVE_GSSHEIMDAL 1

/* if you have the MIT gssapi libraries */
//#define HAVE_GSSMIT 1

/* Define to 1 if you have the `idna_strerror' function. */
//#define HAVE_IDNA_STRERROR 1

/* Define to 1 if you have the `idn_free' function. */
//#define HAVE_IDN_FREE 1

/* Define to 1 if you have the <idn-free.h> header file. */
//#define HAVE_IDN_FREE_H 1

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the `inet_addr' function. */
#define HAVE_INET_ADDR 1

/* Define to 1 if you have the inet_ntoa_r function. */
#define HAVE_INET_NTOA_R 1

/* inet_ntoa_r() takes 2 args */
#define HAVE_INET_NTOA_R_2 1

/* inet_ntoa_r() takes 3 args */
#define HAVE_INET_NTOA_R_3 1

/* Define to 1 if you have a IPv6 capable working inet_ntop function. */
#define HAVE_INET_NTOP 1

/* Define to 1 if you have a IPv6 capable working inet_pton function. */
#define HAVE_INET_PTON 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the ioctl function. */
#define HAVE_IOCTL 1

/* Define to 1 if you have the ioctlsocket function. */
#define HAVE_IOCTLSOCKET 1

/* Define to 1 if you have the IoctlSocket camel case function. */
#define HAVE_IOCTLSOCKET_CAMEL 1

/* Define to 1 if you have a working IoctlSocket camel case FIONBIO function.
   */
#define HAVE_IOCTLSOCKET_CAMEL_FIONBIO 1

/* Define to 1 if you have a working ioctlsocket FIONBIO function. */
#define HAVE_IOCTLSOCKET_FIONBIO 1

/* Define to 1 if you have a working ioctl FIONBIO function. */
#define HAVE_IOCTL_FIONBIO 1

/* Define to 1 if you have a working ioctl SIOCGIFADDR function. */
#define HAVE_IOCTL_SIOCGIFADDR 1

/* Define to 1 if you have the <io.h> header file. */
//#define HAVE_IO_H 1

/* if you have the Kerberos4 libraries (including -ldes) */
//#define HAVE_KRB4 1

/* Define to 1 if you have the `krb_get_our_ip_for_realm' function. */
//#define HAVE_KRB_GET_OUR_IP_FOR_REALM 1

/* Define to 1 if you have the <krb.h> header file. */
//#define HAVE_KRB_H 1

/* Define to 1 if you have the lber.h header file. */
//#define HAVE_LBER_H 1

/* Define to 1 if you have the ldapssl.h header file. */
//#define HAVE_LDAPSSL_H 1

/* Define to 1 if you have the ldap.h header file. */
//#define HAVE_LDAP_H 1

/* Use LDAPS implementation */
//#define HAVE_LDAP_SSL 1

/* Define to 1 if you have the ldap_ssl.h header file. */
//#define HAVE_LDAP_SSL_H 1

/* Define to 1 if you have the `ldap_url_parse' function. */
//#define HAVE_LDAP_URL_PARSE 1

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define to 1 if you have the `idn' library (-lidn). */
//#define HAVE_LIBIDN 1

/* Define to 1 if you have the `resolv' library (-lresolv). */
#define HAVE_LIBRESOLV 1

/* Define to 1 if you have the `resolve' library (-lresolve). */
#define HAVE_LIBRESOLVE 1

/* Define to 1 if you have the `socket' library (-lsocket). */
#define HAVE_LIBSOCKET 1

/* Define to 1 if you have the `ssh2' library (-lssh2). */
//#define HAVE_LIBSSH2 1

/* Define to 1 if libssh2 provides `libssh2_version'. */
//#define HAVE_LIBSSH2_VERSION 1

/* Define to 1 if libssh2 provides `libssh2_init'. */
//#define HAVE_LIBSSH2_INIT 1

/* Define to 1 if libssh2 provides `libssh2_exit'. */
//#define HAVE_LIBSSH2_EXIT 1

/* Define to 1 if libssh2 provides `libssh2_scp_send64'. */
//#define HAVE_LIBSSH2_SCP_SEND64 1

/* Define to 1 if libssh2 provides `libssh2_session_handshake'. */
//#define HAVE_LIBSSH2_SESSION_HANDSHAKE 1

/* Define to 1 if you have the <libssh2.h> header file. */
//#define HAVE_LIBSSH2_H 1

/* Define to 1 if you have the `ssl' library (-lssl). */
#define HAVE_LIBSSL 1

/* if zlib is available */
#define HAVE_LIBZ 1

/* if brotli is available */
//#define HAVE_BROTLI 1

/* if your compiler supports LL */
//#define HAVE_LL 1

/* Define to 1 if you have the <locale.h> header file. */
//#define HAVE_LOCALE_H 1

/* Define to 1 if you have a working localtime_r function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if the compiler supports the 'long long' data type. */
#define HAVE_LONGLONG 1

/* Define to 1 if you have the malloc.h header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the MSG_NOSIGNAL flag. */
{MSG_NOSIGNAL}

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
#define HAVE_NETINET_TCP_H 1

/* Define to 1 if you have the <net/if.h> header file. */
#define HAVE_NET_IF_H 1

/* Define to 1 if NI_WITHSCOPEID exists and works. */
#define HAVE_NI_WITHSCOPEID 1

/* if you have an old MIT gssapi library, lacking GSS_C_NT_HOSTBASED_SERVICE */
//#define HAVE_OLD_GSSMIT 1

/* Define to 1 if you have the <openssl/crypto.h> header file. */
#define HAVE_OPENSSL_CRYPTO_H 1

/* Define to 1 if you have the <openssl/engine.h> header file. */
#define HAVE_OPENSSL_ENGINE_H 1

/* Define to 1 if you have the <openssl/err.h> header file. */
#define HAVE_OPENSSL_ERR_H 1

/* Define to 1 if you have the <openssl/pem.h> header file. */
#define HAVE_OPENSSL_PEM_H 1

/* Define to 1 if you have the <openssl/pkcs12.h> header file. */
#define HAVE_OPENSSL_PKCS12_H 1

/* Define to 1 if you have the <openssl/rsa.h> header file. */
#define HAVE_OPENSSL_RSA_H 1

/* Define to 1 if you have the <openssl/ssl.h> header file. */
#define HAVE_OPENSSL_SSL_H 1

/* Define to 1 if you have the <openssl/x509.h> header file. */
#define HAVE_OPENSSL_X509_H 1

/* Define to 1 if you have the <pem.h> header file. */
#define HAVE_PEM_H 1

/* Define to 1 if you have the `perror' function. */
#define HAVE_PERROR 1

/* Define to 1 if you have the `pipe' function. */
#define HAVE_PIPE 1

/* Define to 1 if you have a working poll function. */
#define HAVE_POLL 1

/* If you have a fine poll */
#define HAVE_POLL_FINE 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have a working POSIX-style strerror_r function. */
#define HAVE_POSIX_STRERROR_R 1

/* Define to 1 if you have the <pthread.h> header file */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* Define to 1 if you have the `RAND_egd' function. */
//#define HAVE_RAND_EGD 1

/* Define to 1 if you have the `RAND_screen' function. */
#define HAVE_RAND_SCREEN 1

/* Define to 1 if you have the `RAND_status' function. */
#define HAVE_RAND_STATUS 1

/* Define to 1 if you have the recv function. */
#define HAVE_RECV 1

/* Define to 1 if you have the recvfrom function. */
#define HAVE_RECVFROM 1

/* Define to 1 if you have the <rsa.h> header file. */
#define HAVE_RSA_H 1

/* Define to 1 if you have the select function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the send function. */
#define HAVE_SEND 1

/* Define to 1 if you have the 'fsetxattr' function. */
#define HAVE_FSETXATTR 1

/* fsetxattr() takes 5 args */
#define HAVE_FSETXATTR_5 1

/* fsetxattr() takes 6 args */
#define HAVE_FSETXATTR_6 1

/* Define to 1 if you have the <setjmp.h> header file. */
#define HAVE_SETJMP_H 1

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the `setmode' function. */
#define HAVE_SETMODE 1

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define to 1 if you have the setsockopt function. */
#define HAVE_SETSOCKOPT 1

/* Define to 1 if you have a working setsockopt SO_NONBLOCK function. */
#define HAVE_SETSOCKOPT_SO_NONBLOCK 1

/* Define to 1 if you have the <sgtty.h> header file. */
#define HAVE_SGTTY_H 1

/* Define to 1 if you have the sigaction function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the siginterrupt function. */
#define HAVE_SIGINTERRUPT 1

/* Define to 1 if you have the signal function. */
#define HAVE_SIGNAL 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the sigsetjmp function or macro. */
#define HAVE_SIGSETJMP 1

/* Define to 1 if sig_atomic_t is an available typedef. */
#define HAVE_SIG_ATOMIC_T 1

/* Define to 1 if sig_atomic_t is already defined as volatile. */
#define HAVE_SIG_ATOMIC_T_VOLATILE 1

/* Define to 1 if struct sockaddr_in6 has the sin6_scope_id member */
#define HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the `SSL_get_shutdown' function. */
#define HAVE_SSL_GET_SHUTDOWN 1

/* Define to 1 if you have the <ssl.h> header file. */
#define HAVE_SSL_H 1

/* Define to 1 if you have the <stdbool.h> header file. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the strcasecmp function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the strcasestr function. */
#define HAVE_STRCASESTR 1

/* Define to 1 if you have the strcmpi function. */
#define HAVE_STRCMPI 1

/* Define to 1 if you have the strdup function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the strerror_r function. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the stricmp function. */
#define HAVE_STRICMP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the strlcat function. */
#define HAVE_STRLCAT 1

/* Define to 1 if you have the `strlcpy' function. */
#define HAVE_STRLCPY 1

/* Define to 1 if you have the strncasecmp function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the strncmpi function. */
#define HAVE_STRNCMPI 1

/* Define to 1 if you have the strnicmp function. */
#define HAVE_STRNICMP 1

/* Define to 1 if you have the <stropts.h> header file. */
//#define HAVE_STROPTS_H 1

/* Define to 1 if you have the strstr function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the strtok_r function. */
#define HAVE_STRTOK_R 1

/* Define to 1 if you have the strtoll function. */
#define HAVE_STRTOLL 1

/* if struct sockaddr_storage is defined */
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* Define to 1 if you have the timeval struct. */
#define HAVE_STRUCT_TIMEVAL 1

/* Define to 1 if you have the <sys/filio.h> header file. */
#define HAVE_SYS_FILIO_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
//#define HAVE_SYS_SOCKIO_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <sys/utime.h> header file. */
#define HAVE_SYS_UTIME_H 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if you have the <termio.h> header file. */
#define HAVE_TERMIO_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the <tld.h> header file. */
#define HAVE_TLD_H 1

/* Define to 1 if you have the `tld_strerror' function. */
#define HAVE_TLD_STRERROR 1

/* Define to 1 if you have the `uname' function. */
#define HAVE_UNAME 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `utime' function. */
#define HAVE_UTIME 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if compiler supports C99 variadic macro style. */
#define HAVE_VARIADIC_MACROS_C99 1

/* Define to 1 if compiler supports old gcc variadic macro style. */
#define HAVE_VARIADIC_MACROS_GCC 1

/* Define to 1 if you have the winber.h header file. */
#define HAVE_WINBER_H 1

/* Define to 1 if you have the windows.h header file. */
//#define HAVE_WINDOWS_H 1

/* Define to 1 if you have the winldap.h header file. */
//#define HAVE_WINLDAP_H 1

/* Define to 1 if you have the winsock2.h header file. */
//#define HAVE_WINSOCK2_H 1

/* Define to 1 if you have the winsock.h header file. */
//#define HAVE_WINSOCK_H 1

/* Define this symbol if your OS supports changing the contents of argv */
//#define HAVE_WRITABLE_ARGV 1

/* Define to 1 if you have the writev function. */
#define HAVE_WRITEV 1

/* Define to 1 if you have the ws2tcpip.h header file. */
//#define HAVE_WS2TCPIP_H 1

/* Define to 1 if you have the <x509.h> header file. */
//#define HAVE_X509_H 1

/* Define if you have the <process.h> header file. */
//#define HAVE_PROCESS_H 1

/* if you have the zlib.h header file */
#define HAVE_ZLIB_H 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR "LT_OBJDIR"

/* If you lack a fine basename() prototype */
//#define NEED_BASENAME_PROTO 1

/* Define to 1 if you need the lber.h header file even with ldap.h */
//#define NEED_LBER_H 1

/* Define to 1 if you need the malloc.h header file even with stdlib.h */
//#define NEED_MALLOC_H 1

/* Define to 1 if _REENTRANT preprocessor symbol must be defined. */
#define NEED_REENTRANT 1

/* cpu-machine-OS */
#define OS "x86_64-gnu-linux"

/* Name of package */
#define PACKAGE libcurl

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT github.com/ndsol/VolcanoSamples

/* Define to the full name of this package. */
#define PACKAGE_NAME libcurl

/* Define to the full name and version of this package. */
#define PACKAGE_STRING libcurl-7_65_0

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME libcurl

/* Define to the version of this package. */
#define PACKAGE_VERSION 7_65_0

/* a suitable file to read random data from */
#define RANDOM_FILE "/dev/urandom"

/* Define to the type of arg 1 for recvfrom. */
#define RECVFROM_TYPE_ARG1 int

/* Define to the type pointed by arg 2 for recvfrom. */
#define RECVFROM_TYPE_ARG2 void

/* Define to 1 if the type pointed by arg 2 for recvfrom is void. */
#define RECVFROM_TYPE_ARG2_IS_VOID 1

/* Define to the type of arg 3 for recvfrom. */
#define RECVFROM_TYPE_ARG3 size_t

/* Define to the type of arg 4 for recvfrom. */
#define RECVFROM_TYPE_ARG4 int

/* Define to the type pointed by arg 5 for recvfrom. */
#define RECVFROM_TYPE_ARG5 struct sockaddr

/* Define to 1 if the type pointed by arg 5 for recvfrom is void. */
//#define RECVFROM_TYPE_ARG5_IS_VOID 1

/* Define to the type pointed by arg 6 for recvfrom. */
#define RECVFROM_TYPE_ARG6 socklen_t

/* Define to 1 if the type pointed by arg 6 for recvfrom is void. */
//#define RECVFROM_TYPE_ARG6_IS_VOID 1

/* Define to the function return type for recvfrom. */
#define RECVFROM_TYPE_RETV ssize_t

/* Define to the type of arg 1 for recv. */
#define RECV_TYPE_ARG1 int

/* Define to the type of arg 2 for recv. */
#define RECV_TYPE_ARG2 void *

/* Define to the type of arg 3 for recv. */
#define RECV_TYPE_ARG3 size_t

/* Define to the type of arg 4 for recv. */
#define RECV_TYPE_ARG4 int

/* Define to the function return type for recv. */
#define RECV_TYPE_RETV ssize_t

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to the type qualifier of arg 5 for select. */
#define SELECT_QUAL_ARG5

/* Define to the type of arg 1 for select. */
#define SELECT_TYPE_ARG1 int

/* Define to the type of args 2, 3 and 4 for select. */
#define SELECT_TYPE_ARG234 fd_set *

/* Define to the type of arg 5 for select. */
#define SELECT_TYPE_ARG5 struct timeval *

/* Define to the function return type for select. */
#define SELECT_TYPE_RETV int

/* Define to the type qualifier of arg 2 for send. */
#define SEND_QUAL_ARG2 const

/* Define to the type of arg 1 for send. */
#define SEND_TYPE_ARG1 int

/* Define to the type of arg 2 for send. */
#define SEND_TYPE_ARG2 void *

/* Define to the type of arg 3 for send. */
#define SEND_TYPE_ARG3 size_t

/* Define to the type of arg 4 for send. */
#define SEND_TYPE_ARG4 int

/* Define to the function return type for send. */
#define SEND_TYPE_RETV ssize_t

/* generated by gen_curl_config.py: */
{SIZEOF_RESULT}

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to the type of arg 3 for strerror_r. */
#define STRERROR_R_TYPE_ARG3 size_t

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define if you want to enable c-ares support */
//#define USE_ARES 1

/* Define if you want to enable POSIX threaded DNS lookup */
#define USE_THREADS_POSIX 1

/* Define if you want to enable WIN32 threaded DNS lookup */
#define USE_THREADS_WIN32 1

/* Define to disable non-blocking sockets. */
#define USE_BLOCKING_SOCKETS 1

/* if GnuTLS is enabled */
//#define USE_GNUTLS 1

/* if PolarSSL is enabled */
//#define USE_POLARSSL 1

/* if Secure Transport is enabled */
//#define USE_SECTRANSP 1

/* if mbedTLS is enabled */
//#define USE_MBEDTLS 1

/* if libSSH2 is in use */
//#define USE_LIBSSH2 1

/* If you want to build curl with the built-in manual */
//#define USE_MANUAL 1

/* if NSS is enabled */
//#define USE_NSS 1

/* if you want to use OpenLDAP code instead of legacy ldap implementation */
//#define USE_OPENLDAP 1

/* if OpenSSL is in use */
#define USE_OPENSSL 1

/* to enable NGHTTP2  */
//#define USE_NGHTTP2 1

/* if Unix domain sockets are enabled  */
#define USE_UNIX_SOCKETS

/* Define to 1 if you are building a Windows target with large file support. */
//#define USE_WIN32_LARGE_FILES 1

/* to enable SSPI support */
//#define USE_WINDOWS_SSPI 1

/* to enable Windows SSL  */
//#define USE_SCHANNEL 1

/* enable multiple SSL backends */
//#define CURL_WITH_MULTI_SSL 1

/* Define to 1 if using yaSSL in OpenSSL compatibility mode. */
//#define USE_YASSLEMUL 1

/* Version number of package */
#define VERSION 7.65.0

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* Define for large files, on AIX-style hosts. */
#define _LARGE_FILES 1

/* define this if you need it to compile thread-safe code */
//#define _THREAD_SAFE $$_THREAD_SAFE

/* Define to empty if `const' does not conform to ANSI C. */
//#define const $$const

/* Type to use in place of in_addr_t when system does not provide it. */
//#define in_addr_t $$in_addr_t

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
/*#ifndef __cplusplus
#undef inline
#endif*/

/* Define to `unsigned int' if <sys/types.h> does not define. */
//#define size_t $$size_t

/* the signed version of size_t */
//#define ssize_t $$ssize_t

/* Define to 1 if you have the mach_absolute_time function. */
//#define HAVE_MACH_ABSOLUTE_TIME 1
'''.format(
  SIZEOF_RESULT=get_sizes(),
  MSG_NOSIGNAL=get_MSG_NOSIGNAL(),
  CURL_CA_BUNDLE=get_CA_BUNDLE(),
  CURL_CA_PATH=get_CA_PATH()))
