//
//  comDetailViewController.m
//  iRadioHD
//
//  Created by Jan Damek on 11.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import "comDetailViewController.h"
#import "GADBannerView.h"
#import "comBannerView.h"

@interface comDetailViewController ()
@property (weak, nonatomic) IBOutlet UIView *adBanner;
@property (strong, nonatomic) UIPopoverController *masterPopoverController;
@property (strong, nonatomic) GADBannerView *banner;

- (void)configureView;
@end

@implementation comDetailViewController

@synthesize adBanner = _adBanner;
@synthesize banner = _banner;
@synthesize radio = _radio;
@synthesize stream = _stream;

#pragma mark - Managing the detail item

- (void)setRadio:(Radio *)newDetailItem
{
    if (_radio != newDetailItem) {
        _radio = newDetailItem;
        
        // Update the view.
        [self configureView];
        
        //TODO: zvoleni vychoziho streamu podle pripojeni
    }

    if (self.masterPopoverController != nil) {
        [self.masterPopoverController dismissPopoverAnimated:YES];
    }        
}
-(void)setStream:(Stream *)streamNew{
    if (_stream != streamNew){
        _stream = streamNew;
        //TODO: rovest nacteni novym streamem
        [self configureView];
    }
}

- (void)configureView
{
    // Update the user interface for the detail item.

    if (self.radio) {
        self.detailDescriptionLabel.text = [self.radio.timeStamp description];
    }
    if (_banner){
        [_banner removeFromSuperview];
        _banner = [comBannerView getBannerView:self];
        [_adBanner addSubview:_banner];
    }
   
}

-(void)viewDidAppear:(BOOL)animated{
    [super viewDidAppear:animated];

    _banner = [comBannerView getBannerView:self];
    [_adBanner addSubview:_banner];

    [self configureView];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

#pragma mark - Split view

- (void)splitViewController:(UISplitViewController *)splitController willHideViewController:(UIViewController *)viewController withBarButtonItem:(UIBarButtonItem *)barButtonItem forPopoverController:(UIPopoverController *)popoverController
{
    barButtonItem.title = NSLocalizedString(@"Master", @"Master");
    [self.navigationItem setLeftBarButtonItem:barButtonItem animated:YES];
    self.masterPopoverController = popoverController;
}

- (void)splitViewController:(UISplitViewController *)splitController willShowViewController:(UIViewController *)viewController invalidatingBarButtonItem:(UIBarButtonItem *)barButtonItem
{
    // Called when the view is shown again in the split view, invalidating the button and popover controller.
    [self.navigationItem setLeftBarButtonItem:nil animated:YES];
    self.masterPopoverController = nil;
}

@end
