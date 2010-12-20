from PIL import Image
import gtk, gobject, threading, time, serial


def toBitmap(filename, size):
	im = Image.open(filename)
	im = im.resize(size)
	print size, im.size
	im = im.point(lambda i: (i>127)*255)
	im = im.convert('1')
	return im
	
def runLengthEncode(im):
	pix = im.load()
	out = []
	for y in range(im.size[1]):
		xstartpos = 0
		pendown = False
		yout = []
		for x in range(im.size[0]):
			if pix[x,y] == 0:
				if not pendown:
					pendown = True
					xstartpos = x
			else:
				if pendown:
					pendown = False
					if y%2 == 1:
						yout.append((y, x, xstartpos))
					else:
						yout.append((y, xstartpos, x))
		if pendown:
			yout.append((y, xstartpos, x))
		if y%2 == 0:
			yout.reverse()
		out += yout
	out.reverse()
	return out

SERIAL_PORT='/dev/ttyUSB1'

FEED_STEP = 40
CARRIAGE_STEP = 2

PAGE_WIDTH = 2200 / CARRIAGE_STEP
PAGE_HEIGHT = 10000 / FEED_STEP

SERVO_DOWN = 790
SERVO_UP = 840
SERVO_NEUTRAL = 1500


class Printer:
	def __init__(self, port, logfn):
		self.port = port
		self.log = logfn
		self.ser = serial.Serial(SERIAL_PORT, 115200, timeout=10)
		
	def serial_write(self, num, cmd):	
		text = '%i%s'%(num,cmd)
		print '>', text 
		while self.ser.inWaiting():
			print self.ser.readline()
		self.ser.write(text)
		self.ser.flush()
		out = self.ser.readline()
		print out
		if len(out) < 5:
			raise IOError("Timeout")

		
	def print_lines(self, lines, status_callback):
		self.pen_out()
		self.position_carriage(100)
		self.feed_paper()
		self.pen_up()
		
		last_y = PAGE_HEIGHT
		
		for i, l in enumerate(lines):
			status_callback(i)
			y, x1, x2 = l
			self.feed(last_y - y)
			self.position_carriage(x1)
			self.draw_to(x2)
			last_y = y
		
		self.pen_out()
		self.feedOut()
		self.position_carriage(0)
		self.log("Done")
		
	def pen_up(self):
		self.log("Raising pen")
		self.serial_write(SERVO_UP, 's')
		
	def pen_down(self):
		self.log("Lowering pen")
		self.serial_write(SERVO_DOWN, 's')
		
	def pen_out(self):
		self.log("Unloading pen")
		self.serial_write(SERVO_NEUTRAL, 's')
		
	def feed_paper(self):
		self.log("Loading paper")
		self.serial_write(0, 'p')
		
	def feed(self, dist=FEED_STEP):
		if dist == 0:
			return
		self.log("Feeding paper")
		self.serial_write(dist*FEED_STEP, 'f')
		
	def feedOut(self):
		self.log("Ejecting paper")
		self.serial_write(0, 'x')
		
	def position_carriage(self, pos):
		self.log("Positioning carriage")
		self.serial_write(pos*CARRIAGE_STEP, 'c')
		
	def draw_to(self, pos):
		self.pen_down()
		self.log("Drawing")
		self.serial_write(pos*CARRIAGE_STEP, 'c')
		self.pen_up()
		
		



class App:
	def __init__(self):
		self.win=gtk.Window()
		self.win.connect("destroy", quit)

		self.area=gtk.DrawingArea()
		self.area.connect("expose_event", self.expose)
		self.area.set_size_request(400, 550)
		
		self.vbox = gtk.VBox(spacing=10)
		self.vbox.pack_start(self.area)
		
		self.status = gtk.Label()
		self.status.set_justify(gtk.JUSTIFY_LEFT)
		self.vbox.pack_start(self.status)
		
		self.hbox = gtk.HBox()
		self.vbox.pack_start(self.hbox)
		
		def make_btn(name, callback):
			b = gtk.Button(name)
			b.connect("clicked", callback)
			self.hbox.pack_start(b)
			
		make_btn("Open Image", self.open_dialog)
		#make_btn("Draw Text", self.open_text)
		make_btn("Print", self.do_print)
		
		self.win.add(self.vbox)
		self.win.show_all()
		
		self.lines = []
		self.currentIndex = 0
		
		self.printer = Printer(SERIAL_PORT, self.log_remote)
		self.print_thread = False
		
	def expose(self, *ignore):
		c = self.area.window.cairo_create()
		
		#erase
		c.set_source_rgb(1,1,1)
		c.paint()
		
		rect = self.area.get_allocation()
		c.scale(float(rect.width)/PAGE_WIDTH, float(rect.height)/PAGE_HEIGHT)
		c.set_line_width(1)
		
		for i, l in enumerate(self.lines):
			if i > self.currentIndex:
				c.set_source_rgb(0.5, 0.5, 0.5)
			elif i == self.currentIndex:
				c.set_source_rgb(1, 0, 0)
			else:
				c.set_source_rgb(0, 0, 0)
				
			y, x1, x2 = l
			c.move_to(x1,  y)
			c.line_to(x2, y)
			c.stroke()
			
	def update(self, currentIndex):
		self.currentIndex = currentIndex
		self.area.window.invalidate_rect(self.area.get_allocation(), False)
		
	def loadImage(self, filename):
		self.log("Loading %s"%filename)
		self.lines = runLengthEncode(toBitmap(filename, (PAGE_WIDTH, PAGE_HEIGHT)))
		self.update(-1)
		self.log("Created %i lines"%len(self.lines))
		
	def log(self, text):
		self.status.set_text(text)
		
	def log_remote(self, text):
		
		gobject.idle_add(self.log, text)
		
	def update_remote(self, pos):
		gobject.idle_add(self.update, pos)
		
	def open_dialog(self, *ignore):
		dialog = gtk.FileChooserDialog(title="Open image",action=gtk.FILE_CHOOSER_ACTION_OPEN,
                                  buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK))                          
		response = dialog.run()
		if response == gtk.RESPONSE_OK:
			self.loadImage(dialog.get_filename())
		dialog.destroy()
		
	def open_text(self, *ignore):
		pass
		
	def do_print(self, *ignore):
		self.print_thread = threading.Thread(target=self.printer.print_lines, args=(self.lines, self.update_remote))
		self.print_thread.start()
		
gtk.gdk.threads_init()
a = App()
a.loadImage('test.png')
gtk.main()
