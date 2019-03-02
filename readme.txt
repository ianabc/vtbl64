The is the readme file for the source code distribution of the rd113.c program
and related slib.c and iocfg.c programs.
See http://www.willsworks.net/file-format/iomega-1-step-backup for full project documentation.
The source code is copyright 2017 by William T. Kranz

These programs are distributed in the hope that they will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Use them at your own risk!

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.


Two web pages are included as documentation:
113-info.htm - a description of program usage for all 3 programs included
113-format.htm - a description of the Iomega version 4.1 1-Step Backup file format
check for updates on my home page above.

There are 3 source code modules, one for each program:
rd113.c - read and display contents of a *.113 backup file
iocfg.c - display contents of *.dbf file associated with backup configuration
slib.c  - display contents of *.lib file found in some Iomega installers

Three build files are supplied
makefile - for linux, use 'make' program to execute
rd113.dos - to build a DOS executable with Open Watcom, use 'wmake -f rd113.dos' to execute
rd113.wnt - to build a Windows NT executable with Open Watcom, use 'wmake -f rd113.wnt' to execute



