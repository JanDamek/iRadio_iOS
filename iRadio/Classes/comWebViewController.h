//
//  comWebViewController.h
//  ABradio
//
//  Created by Jan Damek on /224/12.
//  Copyright (c) 2012 droidsoft.eu. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface comWebViewController : UIViewController <UIWebViewDelegate>{
    UIWebView* webview;
    UIActivityIndicatorView* activity;
    NSString *requestURL;
    UIView *bannerView;
}

@property (atomic, strong) IBOutlet UIWebView *webview;
@property (atomic, strong) IBOutlet UIActivityIndicatorView *activity;
@property (strong, nonatomic) NSString *requestURL;
@property (atomic ,strong) IBOutlet UIView *bannerView;


- (IBAction)backBtn:(id)sender;
- (IBAction)forwardBtn:(id)sender;
- (IBAction)refreshBtn:(id)sender;

@end
