//
//  comCategorieViewController.h
//  iRadioHD
//
//  Created by Jan Damek on 15.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "Categorie.h"

@interface comCategorieViewController : UIViewController

@property (nonatomic, strong) Categorie *cat;
@property (nonatomic, weak) UIPopoverController *popover;

@end
