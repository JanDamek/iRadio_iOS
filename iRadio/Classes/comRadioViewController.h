//
//  comMasterViewController.h
//  iRadioHD
//
//  Created by Jan Damek on 11.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <CoreData/CoreData.h>
#import "Categorie.h"

@interface comRadioViewController : UITableViewController <NSFetchedResultsControllerDelegate, UISearchBarDelegate>

@property (strong, nonatomic) Categorie *detailItem;

@end