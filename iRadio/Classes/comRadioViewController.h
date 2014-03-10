//
//  comMasterViewController.h
//  iRadioHD
//
//  Created by Jan Damek on 11.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <CoreData/CoreData.h>
#import "Radio.h"

@interface comRadioViewController : UITableViewController <NSFetchedResultsControllerDelegate, UISearchBarDelegate>

@property (weak, nonatomic) Categorie *parentItem;

@end
