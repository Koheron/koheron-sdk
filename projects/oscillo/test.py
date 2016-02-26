from config import *

#http.upload_and_install_instrument(zippath)

#a = dvm.write(CONFIG, LED_OFF, 3)

print dvm.read(STATUS, BITSTREAM_ID_OFF)
print dvm.read(STATUS, BITSTREAM_ID_OFF + 4)
print dvm.read(STATUS, BITSTREAM_ID_OFF + 8)
print dvm.read(STATUS, BITSTREAM_ID_OFF + 12)
