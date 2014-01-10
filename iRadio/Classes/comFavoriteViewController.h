//
//  comFavoriteViewController.h
//  ABradio
//
//  Created by Jan Damek on /214/12.
//  Copyright (c) 2012 droidsoft.eu. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface comFavoriteViewController : UITableViewController
{
    NSMutableArray *data;
}
@property (atomic, strong) IBOutlet UIBarButtonItem *btnHraje;
@end
