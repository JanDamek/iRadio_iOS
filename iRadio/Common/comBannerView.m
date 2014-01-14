//
//  comBannerViewController.m
//  BMI Calculator
//
//  Created by Jan Damek on 23.12.13.
//  Copyright (c) 2013 Jan Damek. All rights reserved.
//

#import "comBannerView.h"
#import "comAppDelegate.h"

@interface comBannerView ()

@end


@implementation comBannerView

#define IDIOM    UI_USER_INTERFACE_IDIOM()
#define IPAD     UIUserInterfaceIdiomPad

NSString *MY_BANNER_UNIT_ID = @"ca-app-pub-9508528448741167/5355993450";

+(GADBannerView*)getBannerView:(UIViewController*)rootViewController {
    
    GADBannerView *bannerView_;
    if (IDIOM == IPAD) {
        bannerView_ = [[GADBannerView alloc] initWithAdSize:kGADAdSizeLeaderboard];
    } else
        bannerView_ = [[GADBannerView alloc] initWithAdSize:kGADAdSizeBanner];
        
    bannerView_.adUnitID = MY_BANNER_UNIT_ID;
    
    bannerView_.rootViewController = rootViewController;
    
    GADRequest *r = [GADRequest request];
//    r.testDevices = @[ @"42ead6903d4e5aec64d0e3bbae310e93" ];
    
    [bannerView_ loadRequest:r];
    return  bannerView_;
}

@end
