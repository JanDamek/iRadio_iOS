#import "AsyncImageView.h"

@implementation AsyncImageView

- (void)dealloc {
	[connection cancel]; 
}

- (void)loadImageFromURL:(NSString*)path  list:(NSMutableDictionary*)list{ 
    _list = list;
    _path = path;
    if (![path isEqualToString:@""])
    {
	
    NSURL *url = [NSURL URLWithString:path];    
	NSURLRequest* request = [NSURLRequest requestWithURL:url cachePolicy:NSURLRequestUseProtocolCachePolicy timeoutInterval:60.0];
	connection = [[NSURLConnection alloc] initWithRequest:request delegate:self];
        connection.accessibilityHint = path;
    }else {
        _path = @"";
    }
}

- (void)connection:(NSURLConnection *)theConnection didReceiveData:(NSData *)incrementalData {
	if (data==nil) { data = [[NSMutableData alloc] initWithCapacity:2048]; } 
	[data appendData:incrementalData];
}

- (void)connectionDidFinishLoading:(NSURLConnection*)theConnection {
    NSString* adr=connection.accessibilityHint;
	connection=nil;
	
	UIImage* _image = [UIImage imageWithData:data];
    NSString* _img = [adr stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    _img = [_img stringByReplacingOccurrencesOfString:@"\t" withString:@""];
    
    if (_img && ![_img isEqualToString:@""])
        [_list setObject:_image forKey:_img];
    if (_view!=nil){
        _view.image = _image;
        if (_cell != nil){
            [_cell setNeedsLayout];
        }
    }
    data=nil;
}

- (UIImage*)getImage:(NSString*)path list:(NSMutableDictionary*)list imageView:(UIImageView*)view cell:(UIView *)cell
{
    NSString* img = [path stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    img = [img stringByReplacingOccurrencesOfString:@"\t" withString:@""];
    UIImage *image = [list objectForKey:img]; 
    if (image==nil){
        _view = view;
        _path = img;
        _cell = cell;
        [self loadImageFromURL:img list:list]; 
        return nil;
    }else {
        return image;
    }
}

@end
