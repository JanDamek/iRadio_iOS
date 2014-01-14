//
//  comBannerViewController.h
//  BMI Calculator
//
//  Created by Jan Damek on 23.12.13.
//  Copyright (c) 2013 Jan Damek. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "GADBannerView.h"

@interface comBannerView : NSObject

+(GADBannerView*)getBannerView:(UIViewController*)rootViewController;

@end
