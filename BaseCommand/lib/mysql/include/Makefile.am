# Copyright (C) 2000-2006 MySQL AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA

BUILT_SOURCES =		$(HEADERS_GEN) link_sources
HEADERS_GEN =		mysql_version.h my_config.h
HEADERS_ABI =		mysql.h mysql_com.h mysql_time.h \
			my_list.h my_alloc.h typelib.h mysql/plugin.h
pkginclude_HEADERS =	$(HEADERS_ABI) my_dbug.h m_string.h my_sys.h \
			my_xml.h mysql_embed.h \
		  	my_pthread.h my_no_pthread.h \
			decimal.h errmsg.h my_global.h my_net.h \
			my_getopt.h sslopt-longopts.h my_dir.h \
			sslopt-vars.h sslopt-case.h sql_common.h keycache.h \
			m_ctype.h my_attribute.h $(HEADERS_GEN)
noinst_HEADERS =	config-win.h config-netware.h my_bit.h \
			heap.h my_bitmap.h my_uctype.h \
			myisam.h myisampack.h myisammrg.h ft_global.h\
			mysys_err.h my_base.h help_start.h help_end.h \
			my_nosys.h my_alarm.h queues.h rijndael.h sha1.h \
			my_aes.h my_tree.h my_trie.h hash.h thr_alarm.h \
			thr_lock.h t_ctype.h violite.h my_md5.h base64.h \
			mysql_version.h.in my_handler.h my_time.h \
			my_vle.h my_user.h my_atomic.h atomic/nolock.h \
			atomic/rwlock.h atomic/x86-gcc.h atomic/x86-msvc.h \
			atomic/gcc_builtins.h my_libwrap.h my_stacktrace.h

EXTRA_DIST =        mysql.h.pp mysql/plugin.h.pp

# Remove built files and the symlinked directories
CLEANFILES =            $(BUILT_SOURCES) readline openssl


# Some include files that may be moved and patched by configure
DISTCLEANFILES =	sched.h $(CLEANFILES)

link_sources:
	-$(RM) -f readline openssl
	@readline_h_ln_cmd@
	@yassl_h_ln_cmd@
	echo timestamp > link_sources

# We want both "my_config.h" and "config.h" that are identical, as
# MySQL sources assumes the name "my_config.h", and 3rd party sources
# assumes the name "config.h".
my_config.h: config.h
	$(CP) config.h my_config.h

# These files should not be included in distributions since they are
# generated by configure from the .h.in files
dist-hook:
	$(RM) -f $(distdir)/mysql_version.h $(distdir)/my_config.h

# Don't update the files from bitkeeper
%::SCCS/s.%
