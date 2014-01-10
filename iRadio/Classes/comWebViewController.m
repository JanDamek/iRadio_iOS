//
//  comWebViewController.m
//  ABradio
//
//  Created by Jan Damek on /224/12.
//  Copyright (c) 2012 droidsoft.eu. All rights reserved.
//

#import "comWebViewController.h"
#import "comAppDelegate.h"

@interface comWebViewController ()

@end

@implementation comWebViewController

@synthesize webview, activity, requestURL, bannerView;

- (IBAction)backBtn:(id)sender
{
    [webview goBack];
}

- (IBAction)forwardBtn:(id)sender
{
    [webview goForward];
}

- (IBAction)refreshBtn:(id)sender
{
    [webview reload];
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication]delegate];
    if (!delegate.requestURL || [delegate.requestURL isEqualToString:@""]){
        delegate.requestURL = @"http://www.abradio.cz";
    }
    
    NSURL*url=[NSURL URLWithString:delegate.requestURL];
    NSURLRequest *_request=[NSURLRequest requestWithURL:url];
    [webview loadRequest:_request];
    
    delegate.requestURL = @"";
}

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view.
    
    [webview setDelegate:self];
    [webview setScalesPageToFit:YES];
    if (bannerView){
        GADBannerView *banner = [comAppDelegate getBanner];
        [bannerView addSubview:banner];
    }
    
    UIToolbar *tb = [UIToolbar appearance];
    if (floor(NSFoundationVersionNumber) <= NSFoundationVersionNumber_iOS_6_1) {
        // Load resources for iOS 6.1 or earlier
        [tb setTintColor:[UIColor blackColor]];
    }else{
        [tb setTintColor:[UIColor whiteColor]];
    }
}


#pragma UIWebViewDellegate

-(void)webViewDidStartLoad:(UIWebView *)webView{
    [activity startAnimating];
    activity.hidden= NO;
    NSLog(@"Web View started loading...");
}
-(void)webViewDidFinishLoad:(UIWebView *)webView{
    [activity stopAnimating];
    activity.hidden= TRUE;
    NSLog(@"Web View stop loading...");
}

- (UIStatusBarStyle)preferredStatusBarStyle
{
    return UIStatusBarStyleLightContent;
}

@end
