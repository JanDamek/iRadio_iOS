//
//  comDetailViewController.h
//  iRadioHD
//
//  Created by Jan Damek on 11.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "Radio.h"
#import "Stream.h"

@interface comDetailViewController : UIViewController <UISplitViewControllerDelegate>

@property (strong, nonatomic) Radio *radio;
@property (strong, nonatomic) Stream *stream;

@property (weak, nonatomic) IBOutlet UILabel *detailDescriptionLabel;
@end
