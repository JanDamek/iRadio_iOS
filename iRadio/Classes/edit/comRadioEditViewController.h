//
//  comRadioEditViewController.h
//  iRadioHD
//
//  Created by Jan Damek on 10.03.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "Radio.h"

@interface comRadioEditViewController : UIViewController

@property (nonatomic, strong) Radio *radio;
@property (nonatomic, weak) UIPopoverController *popover;

@end
