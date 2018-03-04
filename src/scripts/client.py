#!/usr/bin/python

from op_socket import Address
from interface import Server, Host
import readline
import sys

ANSI_BOLD = "\x1b[1m"
ANSI_GREEN = "\x1b[32m"
ANSI_RESET = "\x1b[0m"


class CommandManager:
	"""
	The client interface to the server daemon

	"""
	commands = {}
	help_size = 0
	help_text = ""
	keep_going = True
	name = ""
	
	def __init__(self, name, help_size=20, help_text=""):
		self.name = name
		self.help_size = help_size
		self.help_text = help_text
		self.commands = {}
		self.keep_going = True
	
	def new_command(self, cmd):
		self.commands[cmd.selector] = cmd
	
	def help(self):
		print("%sCommands:" % (self.name + ("\n" + self.help_text + "\n" + "\n") if len(self.help_text) > 0 else ""))
		for key in self.commands:
			self.commands[key].print_help()
	
	def commandline(self):
		while self.keep_going:
			k = input("%s%sautogentoo> %s" % (ANSI_GREEN, ANSI_BOLD, ANSI_RESET)).split(" ")
			if not k[0]:
				continue
			try:
				self.commands[k[0]]
			except KeyError:
				print("command '%s' not found" % k[0])
				continue
			target_cmd = self.commands[k[0]]
			if len(k[1:]) != target_cmd.argc:
				print("'%s' requires %s arguments got %s" % (k[0], target_cmd.argc, len(k[1:])))
				continue
			target_cmd.run(k[1:])


class Command:
	selector = ""
	argc = 0
	argv = []
	help = ""
	manager = None
	fptr = None
	
	def __init__(self, manager, selector, func, argv=None, _help=""):
		self.selector = selector
		if argv is None:
			self.argc = 0
		else:
			self.argc = len(argv)
		self.argv = argv
		self.help = _help
		self.manager = manager
		self.fptr = func
	
	def print_help(self):
		if self.argv is None:
			self.argv = []
		print("    %s%s%s%s" % (
			self.selector, " " + ', '.join(self.argv), ' ' * (self.manager.help_size - len(self.selector)), self.help))
	
	def run(self, args):
		self.fptr(*args)


def limit_width(__str, indent, size=60):
	out = ""
	while len(__str) > 0:
		buf = min(size, len(__str))
		out += __str[:buf] + "\n" + " " * indent
		__str = __str[buf:]
	return out[:-1 * (indent + 1)]


def find_host(server: Server, host_id: str) -> [Host, None]:
	for host in server.hosts:
		if host_id == host.get("id"):
			return host
	return None


def print_hosts(server: Server):
	for host in server.hosts:
		print("%s\n%s\nCFLAGS=%s\nUSE=%s\n" % (
			host.get("id"),
			host.get("hostname"),
			limit_width(host.get("cflags"), 7),
			limit_width(host.get("use"), 4, 63)))


def print_host(host: Host):
	print("%s\n%s\nprofile=%s\nchost=%s\narch=%s\nCFLAGS=%s\nUSE=%s\n" % (
		host.get("id"),
		host.get("hostname"),
		host.get("profile"),
		host.get("chost"),
		host.get("arch"),
		limit_width(host.get("cflags"), 7),
		limit_width(host.get("use"), 4, 63))
		)


def rlinput(prompt, prefill=''):
	readline.set_startup_hook(lambda: readline.insert_text(prefill))
	try:
		return input(prompt)
	finally:
		readline.set_startup_hook()


def edit_host(host: Host):
	values = []
	extra = host.get_extra()
	for i, x in enumerate (["hostname", "profile", "cflags", "use"]):
		values.append(host.get(x))
		print("[%d] %s = %s" % (i + 1, x, host.get(x)))
	print("[5] extra = %s" % extra)
	
	k = ""
	f1 = int(input ("Field to edit > "))
	f2 = -1
	last = f1
	if f1 == 5:
		for i, x in enumerate(extra):
			print("[%s] %s" % (i + 1, x))
		f2 = int(input("Field to edit > "))
		values = extra
		last = f2
	else:
		k = ["hostname: ", "profile: ", "cflags: ", "use: "][f1 - 1]
	
	host.set_field(f1, f2 - 1, rlinput(k, values[last - 1]))


def main():
	server = Server(Address("kronos", 9490))
	server.read_server()
	
	cmdline = CommandManager("AutoGentoo CLI", help_text="The autogentoo user interface")
	commands = [
		Command(cmdline, "refresh", lambda: server.read_server(), _help="refresh the server data"),
		Command(cmdline, "help", lambda: cmdline.help(), _help="Print the help page"),
		Command(cmdline, "list", lambda: print_hosts(server), _help="List all the available hosts"),
		Command(cmdline, "host", lambda x: print_host(find_host(server, x)), ["host_id"],
				_help="print all information about specified host"),
		Command(cmdline, "edit", lambda x: edit_host(find_host(server, x)), ["host_id"],
				_help="edit fields in the host given its id"),
		# Command(cmdline, "new", lambda: ),
		Command(cmdline, "exit", exit, _help="exit"),
		Command(cmdline, "q", exit, _help="exit")
	]
	
	for cmd in commands:
		cmdline.new_command(cmd)
	cmdline.commandline()
	
	return 0


exit(main())
