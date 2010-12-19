from PIL import Image
import gtk


def toBitmap(filename, size):
	im = Image.open(filename)
	im.thumbnail(size)
	im = im.point(lambda i: (i>127)*255)
	im = im.convert('1')
	return im
	
def runLengthEncode(im):
	pix = im.load()
	out = []
	for y in range(im.size[1]):
		xstartpos = 0
		pendown = False
		for x in range(im.size[0]):
			if pix[x,y] == 0:
				if not pendown:
					pendown = True
					xstartpos = x
			else:
				if pendown:
					pendown = False
					out.append((y, xstartpos, x))
		if pendown:
			out.append((y, xstartpos, x))
			
	return out


PAGE_WIDTH = 85
PAGE_HEIGHT = 110

class App:
	def __init__(self):
		self.win=gtk.Window()
		self.win.connect("destroy", quit)

		self.area=gtk.DrawingArea()
		self.area.connect("expose_event", self.expose)
		self.area.set_size_request(400, 550)
		
		self.vbox = gtk.VBox()
		self.vbox.pack_start(self.area)
		
		sw = gtk.ScrolledWindow()
		sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
		self.text = gtk.TextView()
		self.text.set_editable(False)
		self.text.set_cursor_visible(False)
		sw.add(self.text)
		self.vbox.pack_start(sw)
		
		self.hbox = gtk.HBox()
		self.vbox.pack_start(self.hbox)
		
		def make_btn(name, callback):
			b = gtk.Button(name)
			b.connect("clicked", callback)
			self.hbox.pack_start(b)
			
		make_btn("Open", self.open_dialog)
		make_btn("Print", self.do_print)
		
		self.win.add(self.vbox)
		self.win.show_all()
		
		self.lines = []
		self.currentIndex = 0
		
	def expose(self, *ignore):
		c = self.area.window.cairo_create()
		
		#erase
		c.set_source_rgb(1,1,1)
		c.paint()
		
		rect = self.area.get_allocation()
		c.scale(float(rect.width)/PAGE_WIDTH, float(rect.height)/PAGE_HEIGHT)
		c.set_line_width(1)
		
		for i, l in enumerate(self.lines):
			if i < self.currentIndex:
				c.set_source_rgb(0.5, 0.5, 0.5)
			elif i == self.currentIndex:
				c.set_source_rgb(1, 0, 0)
			else:
				c.set_source_rgb(0, 0, 0)
				
			y, x1, x2 = l
			c.move_to(x1, y)
			c.line_to(x2, y)
			c.stroke()
			
	def update(self, currentIndex):
		self.currentIndex = currentIndex
		self.expose()
		
	def loadImage(self, filename):
		self.log("Loading %s"%filename)
		self.lines = runLengthEncode(toBitmap(filename, (PAGE_WIDTH, PAGE_HEIGHT)))
		self.lines.reverse()
		self.update(-1)
		self.log("Created %i lines"%len(self.lines))
		
	def clearLog(self, text):
		self.text.get_buffer().set_text('')
		
	def log(self, text):
		buf = self.text.get_buffer()
		end = buf.get_end_iter()
		buf.insert(end, text+'\n')
		self.text.scroll_to_iter(end, 0)
		
	def open_dialog(self, *ignore):
		dialog = gtk.FileChooserDialog(title="Open image",action=gtk.FILE_CHOOSER_ACTION_OPEN,
                                  buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK))                          
		response = dialog.run()
		if response == gtk.RESPONSE_OK:
			self.loadImage(dialog.get_filename())
		dialog.destroy()
		
	def do_print(self, *ignore):
		self.log('Printing...')
		

a = App()
a.loadImage('test.png')
gtk.main()
				
				
		
				
				
		


			
			
	
	
	
