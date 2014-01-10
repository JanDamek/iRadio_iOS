//
//  comPlayerViewController.m
//  ABradio
//
//  Created by Jan Damek on /202/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "comPlayerViewController.h"
#import <UIKit/UIKit.h>
#import <SystemConfiguration/SCNetworkReachability.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <MediaPlayer/MediaPlayer.h>
#import <MediaPlayer/MPMediaItem.h>
#import "AsyncImageView.h"
#import "comWebViewController.h"
#import "comAppDelegate.h"
#import "comContactViewController.h"
#include "GAITracker.h"

NSString *kURLFacebook = @"http://www.facebook.com/pages/ABradiocz/111460366985";

@implementation comPlayerViewController

@synthesize logoImage, description, artImageView, bannerView;
@synthesize sleepBtn, sleepTime, oblibeneBtn, radioLabel, activity, radioSlogan, programBtn;
@synthesize oblibeneMinBtn;

-(IBAction)infoTouch:(id)sender{
        program = YES;
}

-(IBAction)programTouch:(id)sender
{
    program = YES;
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    [_delegate.tracker set:@"WebView" value:[_delegate prg]];
    _delegate.requestURL = [_delegate prg];
}

-(IBAction)play:(id)sender
{
    //prehravac start
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    //[_delegate.player pause];
    
    [_delegate playStart];
    
}

-(IBAction)stop:(id)sender
{
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    [_delegate playStop];
};

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    self.tracker = _delegate.tracker;
    self.screenName = @"Player View";
    
    NSString *lo;
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        lo = @"logo";
    } else lo = @"logo_ipad";
    UIImage *image = [[[AsyncImageView alloc]init]getImage:[_delegate.dataPlayer valueForKey:lo] list:_delegate.imageList imageView:logoImage cell:nil];
    logoImage.image = image;

    CGRect r = [[self volumeView] bounds];
    MPVolumeView *myVolumeView = [[MPVolumeView alloc] initWithFrame: r];
        [myVolumeView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin];
    [[self volumeView] addSubview:myVolumeView];
	// Do any additional setup after loading the view, typically from a nib.
}

-(void) setupOblibeneImage
{
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    if ([_delegate isInOblibene]){
        [oblibeneBtn setHidden:YES];
        [oblibeneMinBtn setHidden:NO];
    }else{
        [oblibeneBtn setHidden:NO];
        [oblibeneMinBtn setHidden:YES];
    }
    
}

- (void)viewDidAppear:(BOOL)animated
{
    program = NO;
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    [super viewDidAppear:animated];
    
    GADBannerView *banner = [comAppDelegate getBanner];
    [bannerView addSubview:banner];
    
    _delegate.description=description;
    _delegate.artImageView=artImageView;
    _delegate.activity=activity;
    _delegate.sleepLabel=sleepTime;
    
    NSString *lo;
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        lo = @"logo";
    } else lo = @"logo_ipad";
    UIImage *image = [[[AsyncImageView alloc]init]getImage:[_delegate.dataPlayer valueForKey:lo] list:_delegate.imageList imageView:logoImage cell:nil];
    logoImage.image = image;
    
    radioLabel.text = [_delegate.dataPlayer valueForKey:@"name"];
    radioSlogan.text = [_delegate.dataPlayer valueForKey:@"description"];
    
    description.text = [_delegate meta];
    
    
    NSString *imgURL;
    if (![[_delegate getArtImage:artImageView] isEqualToString:@""]){
        imgURL = [_delegate getArtImage:artImageView];
        UIImage *image2 = [[[AsyncImageView alloc]init]getImage:imgURL list:_delegate.imageList imageView:artImageView cell:nil];
        artImageView.image = image2;
    }else {
        [artImageView setImage:[UIImage imageNamed:@"vodoznak"]];
    }
    
    [self setupOblibeneImage];
    
    if (_delegate.sleepTimeLeft>0){
        sleepTime.text = [NSString stringWithFormat:@"%d min", _delegate.sleepTimeLeft];}
    else
        sleepTime.text = @"0 min";
    
    [programBtn setHidden:[[_delegate prg] isEqualToString:@""]];
    
}

-(void)viewDidDisappear:(BOOL)animated
{
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    [super viewDidDisappear:animated];
    _delegate.description = nil;
    _delegate.artImageView = nil;
    _delegate.activity = nil;
    _delegate.sleepLabel = nil;
    
    if (!program){
      [self.navigationController popViewControllerAnimated:YES];
    }
    program = NO;
}

- (void)viewDidUnload
{
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    [super viewDidUnload];
    
    _delegate.description = nil;
    _delegate.artImageView = nil;
    _delegate.activity = nil;
    _delegate.sleepLabel = nil;
    _delegate = nil;
    
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self setupOblibeneImage];
}

-(IBAction)sleepTouch:(id)sender;
{
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    [_delegate sleepTouch];
    
    if (_delegate.sleepTimeLeft>0){
        sleepTime.text = [NSString stringWithFormat:@"%d min", _delegate.sleepTimeLeft];}
    else
        sleepTime.text = @"0 min";
    
}

-(IBAction)facebookBtn:(id)sender;
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:kURLFacebook]];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationLandscapeRight); }

-(IBAction)tobleOblibene:(id)sender
{
    comAppDelegate *_delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    [_delegate togleInOblibene];
    [self setupOblibeneImage];
}

@end
