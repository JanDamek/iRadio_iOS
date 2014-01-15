//
//  comEditableTableViewCell.m
//  iRadioHD
//
//  Created by Jan Damek on 15.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import "comEditableTableViewCell.h"

@interface comEditableTableViewCell ()

@property (nonatomic) BOOL isEditing;

-(void)doEditing:(BOOL)editing;

@end

@implementation comEditableTableViewCell

@synthesize isEditing = _isEditing;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if (self) {
        // Initialization code
    }
    return self;
}

- (void)awakeFromNib
{
    // Initialization code
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
    [super setSelected:selected animated:animated];

    // Configure the view for the selected state
}

-(void)setEditing:(BOOL)editing{
    [super setEditing:editing];
    [self doEditing:editing];
}

-(void)setEditing:(BOOL)editing animated:(BOOL)animated{
    [super setEditing:editing animated:animated];
    [self doEditing:editing];
}

-(void)doEditing:(BOOL)editing{
    _isEditing = editing;
}

@end
