#import <UIKit/UIKit.h>

@interface AsyncImageView : NSObject {
	NSURLConnection* connection;
	NSMutableData* data; 
    NSMutableDictionary* _list;
    NSString* _path;
    UIImageView* _view;
    UIView* _cell;
}

- (void)loadImageFromURL:(NSString*)path list:(NSMutableDictionary*)list;
- (UIImage*)getImage:(NSString*)path list:(NSMutableDictionary*)list imageView:(UIImageView*)view cell:(UIView*)cell;

@end