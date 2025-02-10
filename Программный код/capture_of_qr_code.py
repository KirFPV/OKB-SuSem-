import cv2

def take_photo():
    cam = cv2.VideoCapture(1)
    _, im = cam.read()
    del(cam)
    return im
	
def save_photo(im, image_name):
	cv2.imwrite(image_name, im)
	
def capture(image_name):
	image = take_photo()
	save_photo(image, image_name)



if __name__ == '__main__':
    im = take_photo()
    save_photo(im, f'qr.png')